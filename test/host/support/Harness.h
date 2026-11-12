#pragma once
#define _User_H_
#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "ADC.h"
#include "EC11.h"
#include "ui.h"
#include "PWM.h"
#include "eeprom_flash.h"
#include "MIOT.h"

constexpr int UP = 0, DOWN = 1, LEFT = 2, RIGHT = 3;
struct HostOled {
    std::array<std::array<char, 23>, 4> rows{};
    template<class... T> void choose_clr(T...) {}
    template<class... T> void choose_refresh(T...) {}
    template<class... T> void xy_set(T...) {}
    template<class... T> void roll(T...) {}
    template<class... T> void chinese(T...) {}
    template<class... T> void BMP(T...) {}
    template<class... T> void point(T...) {}
    template<class... T> unsigned char num(T...) { return 0; }
    void clr() { for (auto &row : rows) row.fill(0); }
    void str(short, short y, const void *text, unsigned char, unsigned char, unsigned char) {
        if (y >= 0 && y < 32)
            std::snprintf(rows[static_cast<size_t>(y / 8)].data(), 23, "%s", static_cast<const char*>(text));
    }
    void light(unsigned char) {}
    void refresh() {}
    void display_on() {}
    void display_off() {}
};
inline HostOled oled;
struct HostWifiManager {
    String blinker_id;
    bool wifima_flg = false, back_flg = false;
    bool startConfigPortal(const char*) { return false; }
};
inline HostWifiManager wifima;
struct HostWifi {
    int status() const { return 3; }
    String SSID() const { return {}; }
    String psk() const { return {}; }
};
inline HostWifi WiFi;
constexpr int WL_CONNECTED = 3;
struct HostSetWifi { bool power_on_conect() { return false; } };
inline HostSetWifi setwifi;
