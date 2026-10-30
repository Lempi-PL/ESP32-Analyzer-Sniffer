#include "radio_manager.h"
#include "nrf24_device.h"
#include "project_config.h"
#include "spi_bus.h"

#include <Arduino.h>


// Definition also used as `extern` in nrf24_device.cpp
extern const SPISettings NRF_SPI_SETTINGS(8000000, MSBFIRST, SPI_MODE0);

namespace 
{
    NRF24Device nrf1(Config::NRF1::CE, Config::NRF1::CSN);
    NRF24Device nrf2(Config::NRF2::CE, Config::NRF2::CSN);

    constexpr uint8_t REG_CONFIG   = 0x00;
    constexpr uint8_t REG_EN_AA    = 0x01;
    constexpr uint8_t REG_SETUP_AW = 0x03; // MAC address width
    constexpr uint8_t REG_RF_CH    = 0x05;
    constexpr uint8_t REG_RF_SETUP = 0x06;
    constexpr uint8_t REG_STATUS   = 0x07; // Status (interrupt) register
    constexpr uint8_t REG_RPD      = 0x09;
    constexpr uint8_t REG_RX_ADDR_P0 = 0x0A; // MAC Address of Pipe 0
    constexpr uint8_t REG_RX_PW_P0   = 0x11; // Pipe 0 payload width
    constexpr uint8_t REG_FIFO_STATUS= 0x17; // FIFO buffer status

    constexpr uint8_t CMD_W_REGISTER   = 0x20;
    constexpr uint8_t CMD_R_REGISTER   = 0x00;
    constexpr uint8_t CMD_R_RX_PAYLOAD = 0x61; // Reading the payload
    constexpr uint8_t CMD_FLUSH_RX     = 0xE2; // Clearing the RX FIFO

    constexpr uint16_t JAMMER_HIT_THRESHOLD = 40;
    constexpr uint8_t JAMMER_WIDTH_THRESHOLD = 4;
}

std::array<uint16_t, 126> RadioManager::continuousHits = {0};

bool RadioManager::begin() 
{
    nrf1.begin();
    nrf2.begin();
    setMode(RadioMode::SPECTRUM_ANALYZER);
    
    nrf2.writeRegister(REG_CONFIG, 0x0B);
    nrf2.writeRegister(REG_EN_AA, 0x00);
    nrf2.writeRegister(REG_RF_SETUP, 0x0F);
    nrf2.setCE(true);
    delay(2);
    return true;
}

void RadioManager::setMode(RadioMode mode)
{
    nrf1.setCE(false); // Disable RX during configuration

    if (mode == RadioMode::SPECTRUM_ANALYZER) 
    {
        nrf1.writeRegister(REG_CONFIG, 0x0B); // PWR_UP=1, PRIM_RX=1, EN_CRC=1
        nrf1.writeRegister(REG_EN_AA, 0x00);
        nrf1.writeRegister(REG_SETUP_AW, 0x03); // 5-byte MAC (default)
        nrf1.writeRegister(REG_RF_SETUP, 0x0F); // 2Mbps
        nrf1.writeRegister(REG_RX_PW_P0, 0x00); // Disable payload
    } 
    else if (mode == RadioMode::BLE_SNIFFER) 
    {
        nrf1.writeRegister(REG_CONFIG, 0x03); // PWR_UP=1, PRIM_RX=1, EN_CRC=0 (Brak CRC)
        nrf1.writeRegister(REG_EN_AA, 0x00);
        nrf1.writeRegister(REG_SETUP_AW, 0x02); // 4-byte MAC
        nrf1.writeRegister(REG_RF_SETUP, 0x07); // 1Mbps, 0dBm (Standard BLE)
        nrf1.writeRegister(REG_RX_PW_P0, 32);   // Max payload 32 bytes

        // BLE Access Address: 0x8E89BED6 (LSB first)
        SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
        digitalWrite(Config::NRF1::CSN, LOW);
        SpiBus::spi.transfer(CMD_W_REGISTER | REG_RX_ADDR_P0);
        SpiBus::spi.transfer(0xD6);
        SpiBus::spi.transfer(0xBE);
        SpiBus::spi.transfer(0x89);
        SpiBus::spi.transfer(0x8E);
        digitalWrite(Config::NRF1::CSN, HIGH);
        SpiBus::spi.endTransaction();
    }
    else if (mode == RadioMode::NRF_PROMISC) 
    {
        nrf1.writeRegister(REG_CONFIG, 0x03); // No CRC
        nrf1.writeRegister(REG_EN_AA, 0x00);
        nrf1.writeRegister(REG_SETUP_AW, 0x01); // 3-byte MAC (compatibility with clones)
        nrf1.writeRegister(REG_RF_SETUP, 0x0F); // 2Mbps, 0dBm
        nrf1.writeRegister(REG_RX_PW_P0, 32);

        // MAC address set to the preamble (0x00AAAA)
        SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
        digitalWrite(Config::NRF1::CSN, LOW);
        SpiBus::spi.transfer(CMD_W_REGISTER | REG_RX_ADDR_P0);
        SpiBus::spi.transfer(0x00);
        SpiBus::spi.transfer(0xAA);
        SpiBus::spi.transfer(0xAA);
        digitalWrite(Config::NRF1::CSN, HIGH);
        SpiBus::spi.endTransaction();
    }

    // Clearing buffers and interrupt flags
    nrf1.flushRx();
    nrf1.writeRegister(REG_STATUS, 0x70); // Clear RX_DR, TX_DS, MAX_RT
    
    nrf1.setCE(true); // Resume RX
    delayMicroseconds(130); // Time for the PLL lock
}

bool RadioManager::sniffMacLayer(RadioMode mode, MacSniffResult& result) 
{
    static uint8_t bleChannelIdx = 0;
    const uint8_t bleChannels[] = {2, 26, 80}; // BLE broadcast channels (37, 38, 39)
    static uint8_t promiscChannel = 0;

    // Hopping channels
    if (mode == RadioMode::BLE_SNIFFER) 
    {
        result.currentChannel = bleChannels[bleChannelIdx];
        nrf1.writeRegister(REG_RF_CH, result.currentChannel);
        bleChannelIdx = (bleChannelIdx + 1) % 3;
    } 
    else 
    {
        result.currentChannel = promiscChannel;
        nrf1.writeRegister(REG_RF_CH, result.currentChannel);
        promiscChannel = (promiscChannel + 1) % 126;
    }

    delayMicroseconds(130); // Time to tune the synthesizer

    bool packetReceived = false;
    uint8_t fifoStatus = nrf1.readRegister(REG_FIFO_STATUS);

    // Check the RX_EMPTY bit (0 = data available in the buffer)
    if ((fifoStatus & 0x01) == 0) 
    {
        packetReceived = true;

        SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
        digitalWrite(Config::NRF1::CSN, LOW);
        SpiBus::spi.transfer(CMD_R_RX_PAYLOAD);
        for (uint8_t i = 0; i < 32; ++i) 
        {
            result.lastPayload[i] = SpiBus::spi.transfer(0xFF);
        }
        digitalWrite(Config::NRF1::CSN, HIGH);
        SpiBus::spi.endTransaction();

        // Clear the RX_DR flag
        nrf1.writeRegister(REG_STATUS, 0x40);
    }

    return packetReceived;
}

void RadioManager::scanSpectrum(std::array<uint8_t, 126>& buffer) 
{
    // Left in for backward compatibility
    SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);
    for (uint8_t i = 0; i <= 62; ++i) 
    {
        uint8_t ch1 = i;
        uint8_t ch2 = i + 63;

        digitalWrite(Config::NRF1::CSN, LOW);
        SpiBus::spi.transfer(CMD_W_REGISTER | REG_RF_CH);
        SpiBus::spi.transfer(ch1);
        digitalWrite(Config::NRF1::CSN, HIGH);

        digitalWrite(Config::NRF2::CSN, LOW);
        SpiBus::spi.transfer(CMD_W_REGISTER | REG_RF_CH);
        SpiBus::spi.transfer(ch2);
        digitalWrite(Config::NRF2::CSN, HIGH);

        delayMicroseconds(130);

        digitalWrite(Config::NRF1::CSN, LOW);
        SpiBus::spi.transfer(CMD_R_REGISTER | REG_RPD);
        uint8_t rpd1 = SpiBus::spi.transfer(0xFF);
        digitalWrite(Config::NRF1::CSN, HIGH);

        digitalWrite(Config::NRF2::CSN, LOW);
        SpiBus::spi.transfer(CMD_R_REGISTER | REG_RPD);
        uint8_t rpd2 = SpiBus::spi.transfer(0xFF);
        digitalWrite(Config::NRF2::CSN, HIGH);

        if (rpd1 & 0x01) buffer[ch1] = 1;
        if (rpd2 & 0x01) buffer[ch2] = 1;
    }
    SpiBus::spi.endTransaction();
}

void RadioManager::scanAsymmetric(std::array<uint8_t, 126>& buffer, uint8_t targetChannel, SignalProfile& profile) {
    uint16_t nrf2Hits = 0;
    uint16_t nrf2Samples = 0;

    nrf2.writeRegister(REG_RF_CH, targetChannel);
    delayMicroseconds(130);

    // Open a single SPI transaction for the entire scan
    SpiBus::spi.beginTransaction(NRF_SPI_SETTINGS);

    for (uint8_t i = 0; i < 126; ++i) {
        nrf1.writeRegisterFast(REG_RF_CH, i);

        for (uint8_t j = 0; j < 10; ++j) {
            uint8_t rpd2 = nrf2.readRegisterFast(REG_RPD);
            if (rpd2 & 0x01) nrf2Hits++;
            nrf2Samples++;
            delayMicroseconds(10);
        }

        uint8_t rpd1 = nrf1.readRegisterFast(REG_RPD);
        if (rpd1 & 0x01) buffer[i] = 1;
    }

    SpiBus::spi.endTransaction(); // Close the transaction

    profile.channel = targetChannel;
    profile.dutyCycle = (nrf2Samples > 0) ? ((nrf2Hits * 100) / nrf2Samples) : 0;
    profile.type = classifySignal(profile.dutyCycle, targetChannel);
}

SignalType RadioManager::classifySignal(uint8_t dutyCycle, uint8_t channel)
{
    if (dutyCycle == 0) return SignalType::NONE;
    if (dutyCycle > 90) return SignalType::ANALOG_VIDEO; // Continuous Wave / CW Jammer
    
    // BLE advertising channels: 2402 (CH2), 2426 (CH26), 2480 (CH80)
    if (dutyCycle < 15 && (channel == 2 || channel == 26 || channel == 80)) return SignalType::BLE;
    
    if (dutyCycle >= 15 && dutyCycle <= 90) return SignalType::WIFI;
    
    return SignalType::UNKNOWN;
}

JammerState RadioManager::detectJammer(const std::array<uint8_t, 126>& scanBuffer)
{
    JammerState state = {false, 0, 0};
    uint8_t currentStart = 0;
    uint8_t currentEnd = 0;
    uint8_t consecutiveJammedChannels = 0;

    for (uint8_t i = 0; i < 126; ++i)
    {
        if (scanBuffer[i])
        {
            if (continuousHits[i] < 0xFFFF) continuousHits[i]++;
        }
        else
        {
            continuousHits[i] = 0;
        }

        if (continuousHits[i] >= JAMMER_HIT_THRESHOLD)
        {
            if (consecutiveJammedChannels == 0) currentStart = i;
            consecutiveJammedChannels++;
            currentEnd = i;
        }
        else
        {
            if (consecutiveJammedChannels >= JAMMER_WIDTH_THRESHOLD)
            {
                state.detected = true;
                state.startChannel = currentStart;
                state.endChannel = currentEnd;
            }
            consecutiveJammedChannels = 0;
        }
    }

    if (consecutiveJammedChannels >= JAMMER_WIDTH_THRESHOLD)
    {
        state.detected = true;
        state.startChannel = currentStart;
        state.endChannel = currentEnd;
    }

    return state;
}

bool RadioManager::detectFHSS(const std::array<uint8_t, 126>& scanBuffer)
{
    uint8_t isolatedPeaks = 0;
    
    // Searching for single, isolated pulses (characteristic of telemetry/FHSS)
    for (uint8_t i = 1; i < 125; ++i)
    {
        if (scanBuffer[i] && !scanBuffer[i-1] && !scanBuffer[i+1])
        {
            isolatedPeaks++;
        }
    }
    
    // Heuristic: More than 4 isolated spikes in a single scan suggest FHSS
    return isolatedPeaks > 4;
}