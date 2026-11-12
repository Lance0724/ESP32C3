#include <Arduino.h>
#include "Oled.h"
#include "User.h"
/*
  启凡科创QF-HP物联网加热台固件
  版本:V1.5
  日期:2022-5-4
*/

void setup()
{

  delay(100);

#ifdef DEBUG
  {
    Serial.begin(115200);
  }
#endif

  oled.begin();

  eeprom.read_all_data();

  Ticker_init();

  miot.begin();

  ec11.begin(9, 10, 8, ui_key_callb); // SW=9，SA=10，SB=8，右旋增加
  ec11.speed_up(true);
  ec11.speed_up_max(20);

  ui.page_switch_flg = true;
  
}

void loop()
{
  delay(1);
  //if (system_get_cpu_freq() != SYS_CPU_160MHZ)
  //  system_update_cpu_freq(SYS_CPU_160MHZ);
  ui.run_task();
  eeprom.write_task();
  miot.run_task();
}
