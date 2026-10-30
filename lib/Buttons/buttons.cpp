#include <Arduino.h>

#include "project_config.h"
#include "buttons.h"

namespace
{
    constexpr uint8_t DEBOUNCE_TICKS = 2; // 2 ticks * 33 ms = ~66 ms of stability
    constexpr size_t BUTTON_COUNT = 4;

    constexpr TickType_t DEBOUNCE_TIME_MS = pdMS_TO_TICKS(50);

    struct Button
    {
        uint8_t pin;
        bool stableState;
        TickType_t lastChangeTime;
        ButtonState state;
    };

    // Initialize the array
    std::array<Button, BUTTON_COUNT> buttons =
    {{
        {
            Config::Buttons::UP,
            HIGH,
            0,
            { false, false } // pressed, down,
        },
        {
            Config::Buttons::DOWN,
            HIGH,
            0,
            { false, false }
        },
        {
            Config::Buttons::OK,
            HIGH,
            0,
            { false, false }
        },
        {
            Config::Buttons::BACK,
            HIGH,
            0,
            { false, false }
        }
    }};

    void updateButton(Button& button)
    {
        const bool reading = digitalRead(button.pin);
        TickType_t currentTime = xTaskGetTickCount();

        if (reading != button.stableState) 
        {
            if ((currentTime - button.lastChangeTime) >= DEBOUNCE_TIME_MS) 
            {
                button.stableState = reading;
                if (button.stableState == LOW) 
                {
                    button.state.down = true;
                    button.state.pressed = true;
                } 
                else 
                {
                    button.state.down = false;
                }
            }
        } 
        else 
        {
            button.lastChangeTime = currentTime;
        }
    }
}

void Buttons::begin()
{
    pinMode(Config::Buttons::UP, INPUT_PULLUP);
    pinMode(Config::Buttons::DOWN, INPUT_PULLUP);
    pinMode(Config::Buttons::OK, INPUT_PULLUP);
    pinMode(Config::Buttons::BACK, INPUT_PULLUP);

    TickType_t now = xTaskGetTickCount();

    for (Button& button : buttons)
    {
        const bool reading = digitalRead(button.pin);

        button.stableState = reading;
        button.lastChangeTime = now; 

        button.state.pressed = false;
        button.state.down = (reading == LOW);
    }
}

std::array<ButtonState, 4> Buttons::update()
{
    // pressed is an event, therefore it is valid
    // only for the current update cycle.
    for (Button& button : buttons)
    {
        button.state.pressed = false;
    }

    // Update all four buttons.
    for (Button& button : buttons)
    {
        updateButton(button);
    }

    // Return current state of all buttons.
    std::array<ButtonState, 4> states{};

    for (size_t i = 0; i < BUTTON_COUNT; ++i)
    {
        states[i] = buttons[i].state;
    }

    return states;
}