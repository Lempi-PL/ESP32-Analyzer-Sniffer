#include "nrf24_device.h"
#include "spi_bus.h"

#include <Arduino.h>
#include <freertos/task.h>

extern const SPISettings NRF_SPI_SETTINGS; 

NRF24Device::NRF24Device(uint8_t cePin, uint8_t csnPin) 
    : cePin_(cePin), csnPin_(csnPin) {}

void NRF24Device::begin() {
    pinMode(cePin_, OUTPUT);
    pinMode(csnPin_, OUTPUT);
    digitalWrite(cePin_, LOW);
    digitalWrite(csnPin_, HIGH);
}

void NRF24Device::writeRegister(uint8_t reg, uint8_t value) {
    SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
    writeRegisterFast(reg, value);
    SpiBus::spi.endTransaction();
}

uint8_t NRF24Device::readRegister(uint8_t reg) {
    SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
    uint8_t val = readRegisterFast(reg);
    SpiBus::spi.endTransaction();
    return val;
}

void NRF24Device::writeRegisterFast(uint8_t reg, uint8_t value) {
    // ASSERTION: The program will terminate if the task has not locked the mutex
    configASSERT(SpiBus::isLockedByCurrentTask());

    digitalWrite(csnPin_, LOW);
    SpiBus::spi.transfer(0x20 | (reg & 0x1F)); // CMD_W_REGISTER
    SpiBus::spi.transfer(value);
    digitalWrite(csnPin_, HIGH);
}

uint8_t NRF24Device::readRegisterFast(uint8_t reg) {
    // ASSERTION: The program will terminate if the task has not locked the mutex
    configASSERT(SpiBus::isLockedByCurrentTask());
    
    digitalWrite(csnPin_, LOW);
    SpiBus::spi.transfer(0x00 | (reg & 0x1F)); // CMD_R_REGISTER
    uint8_t val = SpiBus::spi.transfer(0xFF);
    digitalWrite(csnPin_, HIGH);
    return val;
}

void NRF24Device::setCE(bool state) {
    digitalWrite(cePin_, state ? HIGH : LOW);
}

void NRF24Device::flushRx() {
    SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
    digitalWrite(csnPin_, LOW);
    SpiBus::spi.transfer(0xE2); // CMD_FLUSH_RX
    digitalWrite(csnPin_, HIGH);
    SpiBus::spi.endTransaction();
}