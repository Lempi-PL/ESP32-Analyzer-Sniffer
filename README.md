# ESP32 Dual-Channel RF Spectrum Analyzer & Sniffer

An advanced, FreeRTOS-based 2.4 GHz RF analysis tool built on the Adafruit Feather ESP32 V2. This project utilizes two nRF24L01+ PA/LNA modules to perform high-speed asymmetric scanning, functioning as a spectrum analyzer, BLE sniffer, and NRF promiscuous packet sniffer with built-in Jammer and FHSS detection capabilities.

## 1. Main Features

* **2.4 GHz Spectrum Analyzer:** Scans 126 channels using asymmetric scanning across dual nRF24L01+ modules for significantly increased resolution and sweep speed.
* **Jammer Detection:** Automatically detects RF jamming attempts and indicates the blocked frequency band.
* **FHSS Detection:** Identifies Frequency-Hopping Spread Spectrum signals (commonly used in drone telemetry and RC controllers).
* **Signal Type Identification:** Capable of distinguishing between Wi-Fi, BLE, and Analog Video signatures.
* **BLE Sniffer:** Scans Bluetooth Low Energy advertising channels (logical 37, 38, 39 / physical 2, 26, 80), featuring payload inspection and a real-time packet counter.
* **NRF Promiscuous Sniffer:** Captures raw RF packets by listening for the standard `0x00AAAA` preamble.
* **FreeRTOS Architecture:** Ensures smooth operation by separating RF processing and UI rendering into dedicated tasks, synchronized via Mutexes and atomic variables.

## 2. Hardware Requirements

* **Microcontroller:** 1x Adafruit Feather ESP32 V2
* **RF Modules:** 2x nRF24L01+ PA/LNA with external antennas
* **Display:** 1x Waveshare 1.32" 128x96 OLED (Part: 24777, Controller: SSD1327, 4-wire SPI interface)
* **Controls:** 4x Tactile Switches (UP, DOWN, OK, BACK)
* **Passive Components:** 
  * 2x 10 µF electrolytic capacitors (>= 10 V)
  * 2x 100 nF ceramic capacitors

## 3. Wiring & Pinout

All SPI devices share a single hardware VSPI bus. Buttons should be wired between the specified GPIO and GND (using internal pull-ups).

| Component | Signal | ESP32 GPIO | Notes |
| :--- | :--- | :--- | :--- |
| **Shared SPI** | SCK | 5 | Connects to NRF1, NRF2, and OLED |
| | MOSI | 19 | Connects to NRF1, NRF2, and OLED |
| | MISO | 21 | Connects to NRF1 and NRF2 (OLED does not use MISO) |
| **NRF24L01+ #1** | CE | 25 | |
| | CSN | 26 | |
| **NRF24L01+ #2** | CE | 14 | |
| | CSN | 13 | |
| **OLED (SSD1327)** | CS | 33 | |
| | DC | 27 | |
| | RST | 32 | |
| **Buttons** | UP | 8 | Connect other leg to GND (`INPUT_PULLUP`) |
| | DOWN | 7 | Connect other leg to GND (`INPUT_PULLUP`) |
| | OK | 4 | Connect other leg to GND (`INPUT_PULLUP`) |
| | BACK | 20 | Connect other leg to GND (`INPUT_PULLUP`) |

## 4. Important Hardware Notes

* **POWER FILTERING (CRITICAL):** The nRF24L01+ PA/LNA modules draw significant current bursts during transmission/scanning. **You must install a 10 µF electrolytic capacitor and a 100 nF ceramic capacitor in parallel** as close to the VCC and GND pins of *each* nRF24 module as possible. Failure to do so will result in brownouts, ESP32 resets, and unstable SPI communication.
* **SHARED SPI BUS:** The OLED and both RF modules operate on the same VSPI bus. Ensure that all Chip Select (CS/CSN) pins are driven `HIGH` during the boot sequence before initializing the SPI bus to prevent device conflicts and bus noise.

## 5. Environment Setup

This project is built using Visual Studio Code and PlatformIO. The firmware requires C++14 or newer to support `std::atomic` and `std::array`.

**Step-by-step installation:**
1. Download and install [Visual Studio Code](https://code.visualstudio.com/).
2. Install the **PlatformIO IDE** extension from the VS Code marketplace.
3. Clone this repository and open the project folder in VS Code.
4. PlatformIO will automatically download the required toolchains and libraries based on the `platformio.ini` file.

**Example `platformio.ini`:**
```ini
[env:adafruit_feather_esp32_v2]
platform = espressif32
board = adafruit_feather_esp32_v2
framework = arduino
monitor_speed = 115200

; Enforce C++14 standard or newer for std::atomic support
build_unflags = -std=gnu++11
build_flags = -std=gnu++14

lib_deps =
    olikraus/U8g2 @ ^2.34.15
```

## 6. Operating Instructions

The device is controlled via the four tactile switches:

* **OK Button:** Toggles through the main operational modes:
  * *Spectrum Analyzer* &rarr; *BLE Sniffer* &rarr; *NRF Promiscuous Sniffer*
* **UP / DOWN Buttons:** 
  * *In Spectrum Analyzer Mode:* Adjusts the "Decay" parameter (range 1-20), which controls the fall-off speed of the spectrum peaks on the display.
* **BACK Button:** Returns to the previous menu or resets the current view (depending on the active context).

## 7. Software Architecture

The firmware is heavily optimized and relies on **FreeRTOS** to maintain real-time performance without display bottlenecking:

* **Task Separation:** The system is divided into a high-priority **RF Task** (handling high-speed SPI polling, channel hopping, and packet parsing for both nRF24 modules) and a lower-priority **UI Task** (handling OLED rendering via the `U8g2` library and button debouncing).
* **Synchronization:** Data exchange between the RF and UI tasks is safely managed using FreeRTOS Mutexes (for larger data buffers) and `std::atomic` variables (for lock-free state updates like decay values and packet counters).
* **SPI Optimization:** Hardware SPI transactions are optimized to ensure that the asymmetric scanning routine of the dual nRF24 modules is not interrupted or delayed by the OLED frame buffer updates.
