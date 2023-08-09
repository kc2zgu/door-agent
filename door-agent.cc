#include <iostream>
#include <fstream>
#include <chrono>
#include <json/json.h>
#include <boost/program_options.hpp>
#include <signal.h>
#include <sys/random.h>
#include <stdlib.h>
#include <uvw.hpp>

#include "Log.hh"
#include "Door.hh"
#include "MqttClient.hh"

using namespace dooragent;
using namespace std;
using namespace std::chrono;
namespace po = boost::program_options;

std::string version{"0.1"};

std::vector<Door> doors;
std::string mqtt_broker, mqtt_prefix, mqtt_ha_prefix, mqtt_dev_prefix;
MqttClient mqtt_client;

constexpr auto poll_time_fast = 250ms;
constexpr auto poll_time_slow = 3s;

void load_config(string config_file)
{
    ifstream config_stream{config_file};

    if (!config_stream.good())
    {
        throw system_error{};
    }

    Json::Value conf_root;
    config_stream >> conf_root;

    auto conf_doors = conf_root["doors"];
    if (conf_doors.type() == Json::arrayValue)
    {
        for (auto& conf_door: conf_doors)
        {
            if (conf_door.isMember("index"))
            {
                auto& new_door = doors.emplace_back(conf_door["index"].asInt());
                if (conf_door.isMember("closed_sensor") && conf_door["closed_sensor"].type() == Json::arrayValue)
                {
                    auto& sensor = conf_door["closed_sensor"];
                    new_door.SetClosedSensor(sensor[0].asString(),
                                             sensor[1].asInt(),
                                             sensor[2].asBool());
                }
                if (conf_door.isMember("open_btn") && conf_door["open_btn"].type() == Json::arrayValue)
                {
                    auto& btn = conf_door["open_btn"];
                    new_door.SetOpenBtn(btn[0].asString(),
                                        btn[1].asInt(),
                                        btn[2].asBool());
                }
                if (conf_door.isMember("open_time"))
                {
                    new_door.SetOpenTime(conf_door["open_time"].asInt());
                }
                if (conf_door.isMember("close_time"))
                {
                    new_door.SetCloseTime(conf_door["close_time"].asInt());
                }
                if (conf_door.isMember("open_start_time"))
                {
                    new_door.SetOpenStartTime(conf_door["open_start_time"].asInt());
                }
            }
        }
    } else {
        Log::Error("No doors defined in configuration");
    }
    auto conf_mqtt = conf_root["mqtt"];
    if (conf_mqtt.type() == Json::objectValue)
    {
        mqtt_broker = conf_mqtt["broker"].asString();
        mqtt_prefix = conf_mqtt["prefix"].asString();
        mqtt_ha_prefix = conf_mqtt["ha_prefix"].asString();
        mqtt_dev_prefix = conf_mqtt["device_prefix"].asString();
    }
}

shared_ptr<uvw::PollHandle> mqtt_start_poll(shared_ptr<uvw::Loop> uvloop, MqttClient *client)
{
    int s = client->GetSocket();
    auto loop_mqtt_poll = uvloop->resource<uvw::PollHandle>(s);
    loop_mqtt_poll->on<uvw::PollEvent>([client](uvw::PollEvent &event, uvw::PollHandle&)
        {
            if (event.flags & uvw::PollHandle::Event::READABLE)
            {
                client->Poll();
                Log::Message("mqtt poll done");
            }
            if (event.flags & uvw::PollHandle::Event::DISCONNECT)
            {
                Log::Message("mqtt: disconnect event");
            }
        });
    loop_mqtt_poll->start(uvw::Flags(uvw::PollHandle::Event::READABLE) | uvw::Flags(uvw::PollHandle::Event::DISCONNECT));
    Log::Message("Started polling mqtt socket " + to_string(s));

    return loop_mqtt_poll;
}

void publish_state(Door& door)
{
    string state_str;
    switch (door.GetState())
    {
    case Door::Open:
        state_str = "open";
        break;
    case Door::Opening:
    case Door::OpeningSensed:
        state_str = "opening";
        break;
    case Door::Closed:
    case Door::OpenStart:
        state_str = "closed";
        break;
    case Door::Closing:
        state_str = "closing";
        break;
    }
    mqtt_client.PublishTopic(mqtt_prefix + to_string(door.GetIndex()) + "/state", state_str, true);
}

void publish_discovery(Door& door)
{
    Json::Value disc(Json::objectValue);
    string index = to_string(door.GetIndex());

    disc["name"] = mqtt_dev_prefix + index;
    disc["unique_id"] = mqtt_dev_prefix + index;
    disc["state_topic"] = mqtt_prefix + index + "/state";
    disc["command_topic"] = mqtt_prefix + index + "/command";
    disc["payload_open"] = "open";
    disc["payload_close"] = "close";

    ostringstream ss;
    ss << disc;

    mqtt_client.PublishTopic(mqtt_ha_prefix + mqtt_dev_prefix + index + "/config", ss.str(), true);
}

int main(int argc, char **argv)
{
    // mosquitto 1.5 uses rand() for client ID, seed it first
    char random_seed[sizeof(int)];
    getrandom(random_seed, sizeof(int), 0);
    int seed = *((int*)random_seed);
    srand(seed);

    po::options_description desc("Options");
    desc.add_options()
        ("help", "Show help message")
        ("version", "Show version information")
        ("config", po::value<string>(), "Main configuration file")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        cout << desc << endl;
        return 0;
    }

    if (vm.count("version"))
    {
        cout << "door-agent version " << version << endl;
        return 0;
    }

    if (vm.count("config"))
    {
        Log::Message("main: loading " + vm["config"].as<string>());
        load_config(vm["config"].as<string>());
    }

    auto uvloop = uvw::Loop::getDefault();
    auto loop_timer = uvloop->resource<uvw::TimerHandle>();
    loop_timer->start(1s, poll_time_fast);
    shared_ptr<uvw::PollHandle> loop_mqtt_poll;
    bool fast_polling = true;

    loop_timer->on<uvw::TimerEvent>([&uvloop, &fast_polling, &loop_timer](uvw::TimerEvent&, uvw::TimerHandle&)
        {
            Log::Trace("Poll timer");
            bool fast_poll_new = false;
            for (auto& door: doors)
            {
                if (door.UpdateState())
                {
                    Log::Message("main: state changed!");
                    publish_state(door);
                }
                if (door.NeedFastPoll())
                {
                    fast_poll_new = true;
                }
            }
            mqtt_client.Poll();
            if (fast_poll_new != fast_polling)
            {
                if (fast_poll_new)
                {
                    loop_timer->repeat(poll_time_fast);
                    Log::Message("main: start fast polling");
                } else
                {
                    loop_timer->repeat(poll_time_slow);
                    Log::Message("main: start slow polling");
                }
                fast_polling = fast_poll_new;
            }
        });

    if (mqtt_client.Connect(mqtt_broker))
    {
        Log::Message("main: MQTT connected to " + mqtt_broker);
        Log::Message("main: MQTT socket: " + std::to_string(mqtt_client.socket()));
        loop_mqtt_poll = mqtt_start_poll(uvloop, &mqtt_client);

        for (auto& door: doors)
        {
            Door *doorp = &door;
            mqtt_client.SubscribeTopic(mqtt_prefix + to_string(door.GetIndex()) + "/command", [doorp](string topic, string payload)
                {
                    Log::Message("MQTT command: " + payload);
                    if (payload == "open")
                    {
                        doorp->DoOpen();
                    }
                    else if (payload == "close")
                    {
                        doorp->DoClose();
                    }
                    publish_state(*doorp);
                });
            publish_discovery(door);
        }
    }

    uvloop->run();
    
    return 0;
}
