#pragma once

#include <array>
#include <stdint.h>

enum class ButtonId : uint8_t
{
    Up = 0,
    Down,
    Ok,
    Back
};

struct ButtonState
{
    bool pressed;
    bool down;
};

class Buttons
{
public:
    static void begin();
    static std::array<ButtonState, 4> update();
};