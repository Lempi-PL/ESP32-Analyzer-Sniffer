#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <SPI.h>


class SpiBus {
public:
    static SemaphoreHandle_t mutex;
    static SPIClass& spi; 

    static void begin();
    static void releaseAll();
    static void lock();
    static void unlock();

    static bool isLockedByCurrentTask();
};