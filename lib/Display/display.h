#pragma once

#include <stdint.h>
#include <U8g2lib.h>

class Display
{
public:
    Display();

    void begin();

    void firstPage(); 
    bool nextPage();  

    void clearBuffer();
    void setFont(const uint8_t* font);
    void setDrawColor(uint8_t color);

    void drawText(
        int16_t x,
        int16_t y,
        const char* text
    );
      void drawHLine(
        int16_t x,
        int16_t y,
        int16_t width
    );

     void drawVLine(
        int16_t x,
        int16_t y,
        int16_t height
    );

    void drawBox(
        int16_t x,
        int16_t y,
        int16_t width,
        int16_t height
    );

    void sendBuffer();

private:
    U8G2_SSD1327_VISIONOX_128X96_2_4W_HW_SPI driver_;
};