#pragma once
#include <stdint.h>

class NRF24Device {
public:
    NRF24Device(uint8_t cePin, uint8_t csnPin);
    void begin();
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    
    void writeRegisterFast(uint8_t reg, uint8_t value);
    uint8_t readRegisterFast(uint8_t reg);

    void setCE(bool state);
    void flushRx();

private:
    uint8_t cePin_;
    uint8_t csnPin_;
};