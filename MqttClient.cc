#include "MqttClient.hh"
#include "Log.hh"
#include <cstring>

using namespace dooragent;
using namespace std;
using namespace std::chrono;

MqttClient::MqttClient()
{

}

MqttClient::~MqttClient()
{
    disconnect();
    Poll();
}

bool MqttClient::Connect(std::string host, int port)
{
    Log::Message("MQTT: connecting to " + host);
    int result = connect(host.c_str(), port, 10);

    if (result == MOSQ_ERR_SUCCESS)
    {
        for (auto& sub: subscriptions)
        {
            auto& topic = sub.first;
            Log::Trace("MQTT: (re)subscribing to " + topic);
            subscribe(nullptr, topic.c_str(), 1);
        }
    }
    else if (result == MOSQ_ERR_INVAL)
        Log::Error("MQTT: invalid connection parameters");
    else if (result == MOSQ_ERR_ERRNO)
        Log::Error("MQTT: system error: " + string{strerror(errno)});

    return (result == MOSQ_ERR_SUCCESS);
}

bool MqttClient::Poll()
{
    Log::Trace("MQTT: polling");
    int ret = loop_read();
    if (ret == MOSQ_ERR_CONN_LOST || ret == MOSQ_ERR_NO_CONN)
    {
        Log::Error("MQTT: connection lost!");
        return false;
    }
    if (want_write())
        loop_write();
    loop_misc();
    return true;
}

int MqttClient::GetSocket()
{
    return socket();
}

void MqttClient::SubscribeTopic(std::string topic)
{
    subscribe(nullptr, topic.c_str(), 1);
    subscriptions[topic].topic = topic;
    Log::Message("MQTT: subscribed to " + topic);
}

void MqttClient::SubscribeTopic(std::string topic, topic_handler handler)
{
    subscribe(nullptr, topic.c_str(), 1);
    subscriptions[topic].topic = topic;
    subscriptions[topic].handler = handler;
    Log::Message("MQTT: subscribed to " + topic + " with handler function");
}

void MqttClient::PublishTopic(std::string topic, std::string payload, bool retain)
{
    publish(nullptr, topic.c_str(), payload.size(), payload.c_str(), 1, retain);
    Log::Trace("MQTT: published " + topic + "=" + payload + (retain ? "[r]" : ""));
}

std::optional<std::string> MqttClient::GetTopicValue(std::string topic) const
{
    auto sub_iter = subscriptions.find(topic);
    if (sub_iter != subscriptions.end())
    {
        auto& sub = sub_iter->second;
        auto age = steady_clock::now() - sub.received;
        if (age < 180s)
            return sub.value;
        else
            Log::Warning("MQTT: not returning stale data for " + topic);
    }
    return nullopt;
}

void MqttClient::on_message(const struct mosquitto_message *message)
{
    if (message->topic != nullptr && message->payload != nullptr)
    {
        std::string topic{message->topic};
        std::string payload{(const char*)message->payload, (size_t)message->payloadlen};
        Log::Trace("MQTT: message: " + topic + "=" + payload);
        auto sub_iter = subscriptions.find(topic);

        if (sub_iter != subscriptions.end())
        {
            auto& sub = sub_iter->second;
            sub.value = payload;
            sub.received = steady_clock::now();

            if (sub.handler)
            {
                Log::Trace("MQTT: Calling handler");
                sub.handler(topic, payload);
            }
        }
    }
}
