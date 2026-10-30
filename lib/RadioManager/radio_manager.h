#pragma once

#include <stdint.h>
#include <array>

struct JammerState 
{
    bool detected;
    uint8_t startChannel;
    uint8_t endChannel;
};

enum class SignalType : uint8_t
{
    NONE = 0,
    WIFI,
    BLE,
    ANALOG_VIDEO,
    FHSS,
    UNKNOWN
};

struct SignalProfile
{
    SignalType type;
    uint8_t dutyCycle; // 0-100%
    uint8_t channel;
};

enum class RadioMode : uint8_t 
{
    SPECTRUM_ANALYZER = 0,
    BLE_SNIFFER,
    NRF_PROMISC
};

struct MacSniffResult 
{
    uint8_t currentChannel;
    uint8_t lastPayload[32];
};

class RadioManager 
{
public:
    static bool begin();
    
    static void scanSpectrum(std::array<uint8_t, 126>& buffer);
    static void scanAsymmetric(std::array<uint8_t, 126>& buffer, uint8_t targetChannel, SignalProfile& profile);
    static JammerState detectJammer(const std::array<uint8_t, 126>& scanBuffer);
    static bool detectFHSS(const std::array<uint8_t, 126>& scanBuffer);

    static void setMode(RadioMode mode);
    static bool sniffMacLayer(RadioMode mode, MacSniffResult& result);

private:
    static SignalType classifySignal(uint8_t dutyCycle, uint8_t channel);

    static std::array<uint16_t, 126> continuousHits;
};