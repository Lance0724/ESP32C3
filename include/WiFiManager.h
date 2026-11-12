#ifndef _wifimanager_H_
#define _wifimanager_H_

#include <Arduino.h>
#include <DNSServer.h>
//#include <ESP8266WiFi.h>
#include <WiFi.h>
//#include <ESP8266WebServer.h>
#include <WebServer.h>
#include "EC11.h"
#include "eeprom_flash.h"
#include "MIOT.h"

class WiFiManager
{

public:
  bool startConfigPortal(const char *apName);

  String blinker_id = "8a6f83f9894c";
  String _ssid = "HiWiFi_GZ";
  String _pass = "123123123";

  bool back_flg = 0;
  bool wifima_flg = 0;
};

extern WiFiManager wifima;

#endif