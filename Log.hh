#ifndef _LOG_HH
#define _LOG_HH

#include <string>

namespace dooragent
{
    enum LogLevel
    {
        Trace,
        Message,
        Warning,
        Error
    };

    class Log
    {
    public:
        static void Trace(std::string text)
            {
                Add(LogLevel::Trace, text);
            }

        static void Message(std::string text)
            {
                Add(LogLevel::Message, text);
            }

        static void Warning(std::string text)
            {
                Add(LogLevel::Warning, text);
            }

        static void Error(std::string text)
            {
                Add(LogLevel::Error, text);
            }

    protected:
        static void Add(LogLevel level, std::string text);
    };
};

#endif
