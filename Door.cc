#include "Door.hh"
#include "Log.hh"
#include <unistd.h>

using namespace dooragent;
using namespace std;

string Door::StateStr(State state)
{
    switch (state)
    {
    case InitSensing:
        return "InitSensing";
    case Closed:
        return "Closed";
    case Open:
        return "Open";
    case OpeningSensed:
        return "OpeningSensed";
    case OpenStart:
        return "OpenStart";
    case Opening:
        return "Opening";
    case Closing:
        return "Closing";
    }
    return "Unknown";
}

Door::Door(int index)
    :current_state(InitSensing), index(index), btn_pulse_time(300)
{
    open_time = 10000;
    close_time = 10000;
    open_start_time = 4000;
    last_state_time = chrono::steady_clock::now();
}

bool Door::SetClosedSensor(std::string chip, int line, bool level)
{
    gpiod::chip io_chip(chip);
    gpio_closed_sensor = io_chip.get_line(line);
    gpiod::line_request req;
    req.consumer = "door:" + to_string(index) + ".sensor";
    req.request_type = gpiod::line_request::DIRECTION_INPUT;
    req.flags = 0;
    gpio_closed_sensor.request(req);
    gpio_closed_level = level;
}

bool Door::SetOpenBtn(std::string chip, int line, bool level)
{
    gpiod::chip io_chip(chip);
    gpio_open_btn = io_chip.get_line(line);
    gpiod::line_request req;
    req.consumer = "door:" + to_string(index) + ".open";
    req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    req.flags = 0;
    gpio_open_btn.request(req);
    gpio_open_level = level;
}

bool Door::SetCloseBtn(std::string chip, int line, bool level)
{

}

void Door::SetOpenTime(int t)
{
    open_time = t;
}

void Door::SetCloseTime(int t)
{
    close_time = t;
}

void Door::SetOpenStartTime(int t)
{
    open_start_time = t;
}

bool Door::UpdateState()
{
    if (!gpio_closed_sensor)
    {
        Log::Error("Door(" + to_string(index) + "): no closed sensor");
        return false;
    }
    int closed_value = gpio_closed_sensor.get_value();
    if (!gpio_closed_level)
        closed_value = !closed_value;

    Log::Message("Door(" + to_string(index) + "): closed=" + to_string(closed_value));

    closed_debounce_input = closed_debounce_input << 1 | (closed_value & 1);
    bool closed_true = (closed_debounce_input & 0xF) == 0xF;
    bool closed_false = (closed_debounce_input & 0xF) == 0;

    auto time_now = chrono::steady_clock::now();

    State new_state = current_state;

    switch (current_state)
    {
    case InitSensing:
        if (closed_true)
        {
            new_state = Closed;
        }
        else if (closed_false)
        {
            new_state = Open;
        }
        break;
    case Closed:
        if (closed_false)
        {
            new_state = OpeningSensed;
        }
        break;
    case Open:
        if (closed_true)
        {
            new_state = Closed;
        }
        break;
    case OpenStart:
        if (closed_false)
        {
            new_state = Opening;
        }
        else if (time_now - last_state_time > open_start_time * 1ms)
        {
            Log::Warning("Door(" + to_string(index) + "): opening timed out");
            new_state = Closed;
        }
        break;
    case Opening:
    case OpeningSensed:
        if (closed_true)
        {
            new_state = Closed;
        }
        else if (time_now - last_state_time > open_time * 1ms)
        {
            new_state = Open;
        }
        break;
    case Closing:
        if (closed_true)
        {
            new_state = Closed;
        }
        else if (time_now - last_state_time > close_time * 1ms)
        {
            Log::Warning("Door(" + to_string(index) + "): closing timed out");
            new_state = Open;
        }
    }

    if (new_state != current_state)
    {
        SetState(new_state);
        return true;
    }
    return false;
}

void Door::SetState(State new_state)
{
    Log::Message("Door(" + to_string(index) + "): state changed " + StateStr(current_state) + " -> " + StateStr(new_state));
    current_state = new_state;
    last_state_time = chrono::steady_clock::now();
}

bool Door::DoOpen()
{
    switch (current_state)
    {
    case Closed:
        SendOpen();
        SetState(OpenStart);
        last_state_time = chrono::steady_clock::now();
        if (gpio_open_btn)
        {
            gpio_open_btn.set_value(gpio_open_level ? 1 : 0);
            usleep(btn_pulse_time * 1000);
            gpio_open_btn.set_value(gpio_open_level ? 0 : 1);
        }
        return true;
    }
    Log::Error("Door(" + to_string(index) + "): can't open in " + StateStr(current_state) + " state");
    return false;
}

bool Door::DoClose()
{
    switch (current_state)
    {
    case Open:
        SendClose();
        SetState(Closing);
        last_state_time = chrono::steady_clock::now();
        if (gpio_open_btn)
        {
            gpio_open_btn.set_value(gpio_open_level ? 1 : 0);
            usleep(btn_pulse_time * 1000);
            gpio_open_btn.set_value(gpio_open_level ? 0 : 1);
        }
        return true;
    }
    Log::Error("Door(" + to_string(index) + "): can't close in " + StateStr(current_state) + " state");
    return false;
}

void Door::SendOpen()
{
    
}

void Door::SendClose()
{
    
}
