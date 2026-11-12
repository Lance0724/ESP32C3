#pragma once
#include "Arduino.h"
constexpr const char* BLINKER_CMD_ON = "on";
constexpr const char* BLINKER_CMD_OFF = "off";
constexpr uint8_t BLINKER_CMD_MIOT_DAY = 1, BLINKER_CMD_MIOT_NIGHT = 2,
    BLINKER_CMD_MIOT_COLOR = 3, BLINKER_CMD_MIOT_WARMTH = 4,
    BLINKER_CMD_MIOT_TV = 5, BLINKER_CMD_MIOT_READING = 6, BLINKER_CMD_MIOT_COMPUTER = 7;
struct BlinkerButton {
    explicit BlinkerButton(const char*) {}
    void attach(void (*)(const String&)) {}
};
struct BlinkerSlider {
    explicit BlinkerSlider(const char*) {}
    void attach(void (*)(int32_t)) {}
};
struct HostBlinker {
    template<class... T> void begin(T...) {}
    void run() {}
};
inline HostBlinker Blinker;
struct HostBlinkerMiot {
    template<class T> void attachPowerState(T) {}
    template<class T> void attachColor(T) {}
    template<class T> void attachMode(T) {}
};
inline HostBlinkerMiot BlinkerMIOT;
struct HostBlinkerDebug { template<class T> void stream(T&) {} };
inline HostBlinkerDebug BLINKER_DEBUG;
