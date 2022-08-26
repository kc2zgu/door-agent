#ifndef _DOOR_HH
#define _DOOR_HH

#include <chrono>
#include <string>
#include <gpiod.hpp>

namespace dooragent
{
    class Door
    {
    public:
        enum State
        {
            InitSensing,
            Closed,
            Open,
            OpeningSensed,
            OpenStart,
            Opening,
            Closing
        };

        Door(int index);

        bool SetClosedSensor(std::string chip, int line, bool level);
        bool SetOpenBtn(std::string chip, int line, bool level);
        bool SetCloseBtn(std::string chip, int line, bool level);
        void SetOpenTime(int t);
        void SetCloseTime(int t);
        void SetOpenStartTime(int t);

        int GetIndex() const { return index; }
        State GetState() const { return current_state; }
        bool GetFault() const { return fault; }

        bool UpdateState();

        bool DoOpen();
        bool DoClose();

        static std::string StateStr(State state);

    protected:
        int index;
        State current_state;
        bool closed;
        bool fault;
        unsigned int closed_debounce_input;

        void SetState(State new_state);

        void SendOpen();
        void SendClose();

        gpiod::line gpio_closed_sensor, gpio_open_btn, gpio_close_btn;
        bool gpio_closed_level, gpio_open_level, gpio_close_level;
        int btn_pulse_time, open_time, close_time, open_start_time;
        std::chrono::steady_clock::time_point last_state_time;
    };
};

#endif
