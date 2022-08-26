#include "Log.hh"

#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

using namespace dooragent;
using namespace std;

void Log::Add(LogLevel level, string text)
{
    stringstream line;

    auto now = chrono::system_clock::now();
    auto now_time_t = chrono::system_clock::to_time_t(now);

    line << put_time(localtime(&now_time_t), "%T") << " ";

    switch (level)
    {
    case LogLevel::Trace:
        line << "[TT]";
        break;
    case LogLevel::Message:
        line << "[MM]";
        break;
    case LogLevel::Warning:
        line << "[WW]";
        break;
    case LogLevel::Error:
        line << "[EE]";
        break;
    }

    line << " " << text;

    cout << line.str() << endl;
}
