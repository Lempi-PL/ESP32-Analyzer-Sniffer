#pragma once

#include <stdint.h>

namespace Config 
{
    
    namespace SPI 
    {
        constexpr uint8_t SCK  = 5;
        constexpr uint8_t MOSI = 19;
        constexpr uint8_t MISO = 21;
    }
    
    namespace NRF1
    {
        constexpr uint8_t CE  = 25;
        constexpr uint8_t CSN = 26;
    }

    namespace NRF2
    {
        constexpr uint8_t CE  = 14;
        constexpr uint8_t CSN = 13;
    }

    namespace OLED
    {
        constexpr uint8_t CS  = 33;
        constexpr uint8_t DC  = 27;
        constexpr uint8_t RST = 32;
    }

    namespace Buttons
    {
        constexpr uint8_t UP   = 8;
        constexpr uint8_t DOWN = 7;
        constexpr uint8_t OK   = 4;
        constexpr uint8_t BACK = 20;
    }
}