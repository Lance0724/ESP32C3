#ifndef _User_H_
#define _User_H_

#define DEBUG


#include <Arduino.h>
#include <Ticker.h>
/*#ifdef esp32c3
  #include <HttpClient.h>
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266HTTPClient.h>
  #include <ESP8266WiFi.h>
#endif
*/

//#include <HttpClient.h>
#include <WiFi.h>

#include <WiFiClientSecure.h>
#include <EEPROM.h>


#include "Oled.h"
#include "ui.h"
#include "EC11.h"

#include "Tick_IRQ.h"
#include "set_wifi.h"
#include "eeprom_flash.h"
#include "adc.h"
#include "PWM.h"
#include "MIOT.h"
#include "WiFiManager.h"


#endif