#include "spi_bus.h"
#include "project_config.h"

#include <Arduino.h>
#include <freertos/task.h>

SemaphoreHandle_t SpiBus::mutex = nullptr;
SPIClass& SpiBus::spi = SPI; 

void SpiBus::begin() 
{
    mutex = xSemaphoreCreateMutex();
    
    pinMode(Config::NRF1::CSN, OUTPUT);
    pinMode(Config::NRF2::CSN, OUTPUT);
    pinMode(Config::OLED::CS, OUTPUT);
    pinMode(Config::NRF1::CE, OUTPUT);
    pinMode(Config::NRF2::CE, OUTPUT);
    
    releaseAll();
    digitalWrite(Config::NRF1::CE, LOW);
    digitalWrite(Config::NRF2::CE, LOW);

    spi.begin(Config::SPI::SCK, Config::SPI::MISO, Config::SPI::MOSI);
}

void SpiBus::lock() {
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void SpiBus::unlock() {
    xSemaphoreGive(mutex);
}

void SpiBus::releaseAll() 
{
    digitalWrite(Config::NRF1::CSN, HIGH);
    digitalWrite(Config::NRF2::CSN, HIGH);
    digitalWrite(Config::OLED::CS, HIGH);
}


bool SpiBus::isLockedByCurrentTask() 
{
    if (mutex == nullptr) {
        return false;
    }
    
    // Retrieve a Taska handle that holds a mutex
    TaskHandle_t holder = xSemaphoreGetMutexHolder(mutex);
    
    // Retrieve the handle for the currently running task
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    
    return (holder == currentTask);
}
