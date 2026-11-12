#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using String = std::string;
using byte = uint8_t;
constexpr int OUTPUT = 1, INPUT_PULLUP = 2, CHANGE = 3, LOW = 0, HIGH = 1;
#define ICACHE_RAM_ATTR
#define INPUT 0x01
#define IRAM_ATTR
inline uint32_t host_now = 0;
inline uint32_t host_adc_mv = 2520;
inline bool host_isr = false;
inline std::array<int, 32> host_pwm{};
inline std::array<int, 32> host_io{};
inline uint32_t millis() { return host_now; }
inline void delay(uint32_t ms) { host_now += ms; }
inline void yield() {}
inline void pinMode(int, int) {}
inline void digitalWrite(int pin, int value) { host_io.at(pin) = value; }
inline int digitalRead(int pin) { return host_io.at(pin); }
inline void analogWrite(int pin, int duty) { host_pwm.at(pin) = duty; }
inline uint32_t analogReadMilliVolts(int) { return host_adc_mv; }
inline uint16_t analogRead(int) { return static_cast<uint16_t>(host_adc_mv); }
inline void attachInterrupt(int, void (*)(), int) {}
inline void detachInterrupt(int) {}
inline bool xPortInIsrContext() { return host_isr; }
struct HostSerial {
    void begin(int) {}
    template<class T> void print(const T&) {}
    template<class T> void println(const T&) {}
};
inline HostSerial Serial;
