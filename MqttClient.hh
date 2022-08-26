#ifndef _MQTTCLIENT_HH
#define _MQTTCLIENT_HH

#include <mosquittopp.h>
#include <string>
#include <optional>
#include <map>
#include <functional>
#include <chrono>

namespace dooragent
{
    class MqttClient : public mosqpp::mosquittopp
    {
        using topic_handler = std::function<void(std::string,std::string)>;

    public:
        MqttClient();
        ~MqttClient();

        bool Connect(std::string host, int port = 1883);
        bool Poll();
        int GetSocket();

        void SubscribeTopic(std::string topic);
        void SubscribeTopic(std::string topic, topic_handler);
        void PublishTopic(std::string topic, std::string payload, bool retain = false);
        std::optional<std::string> GetTopicValue(std::string topic) const;

        void on_message(const struct mosquitto_message *message);

        struct subscription
        {
            std::string topic;
            std::optional<std::string> value;
            topic_handler handler;
            std::chrono::steady_clock::time_point received;
        };

    protected:
        std::map<std::string, subscription> subscriptions;

    };

};

#endif
