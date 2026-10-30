#include "spi_bus.h"
#include "radio_manager.h"
#include "buttons.h"
#include "display.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <atomic>



Display display;

constexpr uint8_t MAX_BAR_HEIGHT = 80;
constexpr uint8_t HIT_WEIGHT = 15;

// A structure that extends data from the MAC layer with a packet counter for the UI
struct MacSniffDataUI {
    uint32_t packetCount;
    uint8_t currentChannel;
    uint8_t lastPayload[32];
};

struct AppState {
    std::array<uint8_t, 126> spectrumData;
    MacSniffDataUI macData;
    bool jammerDetected;
    uint8_t jammerStartCh;
    uint8_t jammerEndCh;
    SignalProfile currentProfile;
    bool fhssDetected;
};

// Atomic variables
std::atomic<RadioMode> currentMode{RadioMode::SPECTRUM_ANALYZER};
std::atomic<uint8_t> currentDecayRate{3};
std::atomic<bool> resetDataFlag{false}; // Flag to reset buffers when the mode changes

AppState sharedState;
SemaphoreHandle_t stateMutex;

void rfTask(void *pvParameters);
void uiTask(void *pvParameters);

void setup()
{
    Serial.begin(115200);

    stateMutex = xSemaphoreCreateMutex();
    if (stateMutex == NULL) {
        Serial.println("[FATAL] Mutex creation failed!");
        while (true);
    }

    SpiBus::begin();
    
    SpiBus::lock();
    RadioManager::begin();
    display.begin();
    display.clearBuffer();
    display.setFont(u8g2_font_6x10_tf);
    display.drawText(0, 10, "BOOTING...");
    display.sendBuffer();
    SpiBus::unlock();

    Buttons::begin();

    xTaskCreatePinnedToCore(rfTask, "RF_TASK", 4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(uiTask, "UI_TASK", 4096, NULL, 1, NULL, 1);

    vTaskDelete(NULL);
}

void loop() {}

void rfTask(void *pvParameters) {
    std::array<uint8_t, 126> localBuffer;
    uint8_t targetWatchdogChannel = 40;
    AppState localState; 

    // Initialize the local state to zero
    localState.spectrumData.fill(0);
    localState.macData = {0, 0, {0}};

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20); // Deterministic 20 ms

    while (true) {
        // Reading control parameters from atomic variables (without a mutex)
        RadioMode mode = currentMode.load();
        uint8_t decay = currentDecayRate.load();

        // Handling a data reset request (e.g., after the user changes the mode)
        if (resetDataFlag.exchange(false)) {
            localState.spectrumData.fill(0);
            localState.macData.packetCount = 0;
            memset(localState.macData.lastPayload, 0, 32);
        }

        if (mode == RadioMode::SPECTRUM_ANALYZER) {
            localBuffer.fill(0);
            SignalProfile localProfile;

            SpiBus::lock();
            RadioManager::scanAsymmetric(localBuffer, targetWatchdogChannel, localProfile);
            SpiBus::unlock();

            JammerState jState = RadioManager::detectJammer(localBuffer);
            bool localFhss = RadioManager::detectFHSS(localBuffer);

            uint8_t maxEnergy = 0;
            uint8_t maxEnergyChannel = targetWatchdogChannel;

            // The DECAY app and adding new hits to the LOCAL copy
            for (uint8_t i = 0; i < 126; ++i) {
                // 1. Decay
                if (localState.spectrumData[i] >= decay) {
                    localState.spectrumData[i] -= decay;
                } else {
                    localState.spectrumData[i] = 0;
                }

                // 2. Adding new hits
                if (localBuffer[i]) {
                    localState.spectrumData[i] = min((int)localState.spectrumData[i] + HIT_WEIGHT, (int)MAX_BAR_HEIGHT);
                }

                // 3. Finding the channel with the highest energy for an asymmetric scanner
                if (localState.spectrumData[i] > maxEnergy) {
                    maxEnergy = localState.spectrumData[i];
                    maxEnergyChannel = i;
                }
            }
            
            targetWatchdogChannel = maxEnergyChannel;
            localState.jammerDetected = jState.detected;
            localState.jammerStartCh = jState.startChannel;
            localState.jammerEndCh = jState.endChannel;
            localState.currentProfile = localProfile;
            localState.fhssDetected = localFhss;
        } 
        else {
            MacSniffResult localResult;
            SpiBus::lock();
            bool hit = RadioManager::sniffMacLayer(mode, localResult);
            SpiBus::unlock();

            localState.macData.currentChannel = localResult.currentChannel;
            if (hit) {
                localState.macData.packetCount++;
                memcpy(localState.macData.lastPayload, localResult.lastPayload, 32);
            }
        }

        // Instant synchronization of state to the global state (write-only)
        // rfTask is the ONLY data producer for AppState.
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        sharedState = localState; 
        xSemaphoreGive(stateMutex);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}


void drawUI(const AppState& localState, RadioMode mode, uint8_t decayRate)
{
    char uiBuffer[32];
    
    if (mode == RadioMode::SPECTRUM_ANALYZER) 
    {
        for (uint8_t i = 0; i < 126; ++i)
        {
            if (localState.spectrumData[i] > 0)
            {
                display.drawVLine(i + 1, 95 - localState.spectrumData[i], localState.spectrumData[i]);
            }
        }

        display.setDrawColor(0);
        display.drawBox(0, 0, 128, 22);
        display.setDrawColor(1);
        
        if (localState.jammerDetected)
        {
            snprintf(uiBuffer, sizeof(uiBuffer), "JAMMER: CH%d-CH%d!", localState.jammerStartCh, localState.jammerEndCh);
            display.drawText(2, 10, uiBuffer);
            display.drawHLine(localState.jammerStartCh + 1, 24, (localState.jammerEndCh - localState.jammerStartCh) + 1);
        }
        else
        {
            const char* typeStr = "UNK";
            switch(localState.currentProfile.type) {
                case SignalType::WIFI: typeStr = "WIFI"; break;
                case SignalType::BLE: typeStr = "BLE"; break;
                case SignalType::ANALOG_VIDEO: typeStr = "VID"; break;
                case SignalType::NONE: typeStr = "---"; break;
                default: break;
            }
            
            snprintf(uiBuffer, sizeof(uiBuffer), "CH%d %d%% [%s]", 
                     localState.currentProfile.channel, localState.currentProfile.dutyCycle, typeStr);
            display.drawText(2, 10, uiBuffer);

            snprintf(uiBuffer, sizeof(uiBuffer), "FHSS:%s D:%d", 
                     localState.fhssDetected ? "YES" : "NO", decayRate);
            display.drawText(2, 20, uiBuffer);
        }
    } 
    else 
    {
        display.setDrawColor(1);
        
        // Using the “mode” argument 
        if (mode == RadioMode::BLE_SNIFFER) {
            display.drawText(2, 10, "MODE: BLE SNIFFER");
        } else {
            display.drawText(2, 10, "MODE: NRF PROMISC");
        }
        
        snprintf(uiBuffer, sizeof(uiBuffer), "SCAN CH: %d", localState.macData.currentChannel);
        display.drawText(2, 25, uiBuffer);
        
        snprintf(uiBuffer, sizeof(uiBuffer), "PKTS: %u", localState.macData.packetCount);
        display.drawText(2, 40, uiBuffer);
        
        display.drawText(2, 55, "LAST PAYLOAD:");
        
        snprintf(uiBuffer, sizeof(uiBuffer), "%02X %02X %02X %02X %02X %02X", 
                 localState.macData.lastPayload[0], localState.macData.lastPayload[1],
                 localState.macData.lastPayload[2], localState.macData.lastPayload[3],
                 localState.macData.lastPayload[4], localState.macData.lastPayload[5]);
        display.drawText(2, 70, uiBuffer);
        
        snprintf(uiBuffer, sizeof(uiBuffer), "%02X %02X %02X %02X %02X %02X", 
                 localState.macData.lastPayload[6], localState.macData.lastPayload[7],
                 localState.macData.lastPayload[8], localState.macData.lastPayload[9],
                 localState.macData.lastPayload[10], localState.macData.lastPayload[11]);
        display.drawText(2, 85, uiBuffer);
    }
}


void uiTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(33); // ~30 FPS
    
    while (true)
    {
        AppState localState;
        auto btnState = Buttons::update();
        bool modeChanged = false;

        // =========================================================
        // 1. BUTTON LOGIC (Atomic variables)
        // =========================================================
        RadioMode currentM = currentMode.load();
        
        if (btnState[2].pressed) // OK button - change mode
        {
            currentM = static_cast<RadioMode>((static_cast<uint8_t>(currentM) + 1) % 3);
            currentMode.store(currentM);
            resetDataFlag.store(true); // Command to rfTask to clear the buffers
            modeChanged = true;
        }

        if (currentM == RadioMode::SPECTRUM_ANALYZER) 
        {
            uint8_t decay = currentDecayRate.load();
            // std::min / std::max with explicit typing
            if (btnState[0].pressed) currentDecayRate.store(std::min<uint8_t>(decay + 1, 20)); // UP
            if (btnState[1].pressed) currentDecayRate.store(std::max<uint8_t>(decay - 1, 1));  // DOWN
        }

        // =========================================================
        // 2. QUICK STATUS READ (Critical Section)
        // =========================================================
        xSemaphoreTake(stateMutex, portMAX_DELAY);
        localState = sharedState; 
        xSemaphoreGive(stateMutex);

        // =========================================================
        // 3. HARDWARE OPERATIONS (SPI) — WITHOUT A STATE MUTEX!
        // =========================================================
        if (modeChanged) 
        {
            SpiBus::lock();
            RadioManager::setMode(currentM);
            SpiBus::unlock();
        }

        // =========================================================
        // 4. UI RENDERING (Picture Loop - without blocking RF!)
        // =========================================================
        SpiBus::lock();
        display.firstPage();
        SpiBus::unlock();

        bool morePages = true;
        do {
            // 1. Drawing to RAM
            drawUI(localState, currentM, currentDecayRate.load());
            
            // 2. Sending the rendered page to the OLED
            SpiBus::lock();
            morePages = display.nextPage();
            SpiBus::unlock();
            
            // 3. Forcing a context switch
            taskYIELD();
            
        } while (morePages);

        // Maintaining a fixed time interval
        vTaskDelayUntil(&xLastWakeTime, xFrequency); 
    }
}