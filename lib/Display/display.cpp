#include "project_config.h"
#include "display.h"


Display::Display()

    : driver_(
        U8G2_R0,
        Config::OLED::CS,
        Config::OLED::DC,
        Config::OLED::RST
    )
{}


void Display::begin()
{
    driver_.begin();
}

void Display::firstPage()
{
    driver_.firstPage();
}

bool Display::nextPage()
{
    return driver_.nextPage();
}

void Display::clearBuffer()
{
    driver_.clearBuffer();
}


void Display::setFont(const uint8_t* font)
{
    driver_.setFont(font);
}


void Display::setDrawColor(uint8_t color)
{
    driver_.setDrawColor(color);
}


void Display::drawText(
    int16_t x,
    int16_t y,
    const char* text
)
{
    driver_.drawStr(x, y, text);
}


void Display::drawHLine(
    int16_t x,
    int16_t y,
    int16_t width
)
{
    driver_.drawHLine(x, y, width);
}


void Display::drawVLine(
    int16_t x,
    int16_t y,
    int16_t height
)
{
    driver_.drawVLine(x, y, height);
}


void Display::drawBox(
    int16_t x,
    int16_t y,
    int16_t width,
    int16_t height
)
{
    driver_.drawBox(x, y, width, height);
}


void Display::sendBuffer()
{
    driver_.sendBuffer();
}