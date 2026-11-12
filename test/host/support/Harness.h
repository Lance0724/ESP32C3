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
#include "Oled.h"

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
