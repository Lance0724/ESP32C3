#include "ADC.h"
#include <Ticker.h>
#include "EC11.h"


//#define vin4096                2770      //4096时的电压直mV 250/316
#define vin                    3300      //电源电压，上拉电阻电源段电压mV

#define downRESISTANCE         20000     //并联下拉降压电阻，
#define REFERENCE_RESISTANCE   5100 //上拉电阻
#define NOMINAL_RESISTANCE     100000 // NTC 阻直
#define NOMINAL_TEMPERATURE    25       //
#define B_VALUE                3950
#define ESP32_ANALOG_RESOLUTION   4096 //8位 ACD 1024，12位 ACD 4096


#define temp_w 200 //采样容差宽度+—30
//#define ji     100        
double time_arr[8];
double Vout = 0.0;
double vm = 0.0;
int i_forV = 0;


Thermistor* thermistor;

ADC adc;
Ticker adc_max_temp_tic;

ADC::ADC()
{
    //pinMode(switch_io, OUTPUT);
    //digitalWrite(switch_io, channel_low_temp);
    thermistor = new NTC_Thermistor(
        TEMP_PIN,
        REFERENCE_RESISTANCE,
        NOMINAL_RESISTANCE,
        NOMINAL_TEMPERATURE,
        B_VALUE,
        ESP32_ANALOG_RESOLUTION
    );
    //开机时,初始化温度数组数据
    /*for (int i = 0; i < 10; i++) {
      time_arr[i] = thermistor->readCelsius();
      item+=time_arr[i];
    }*/
      for (int i = 0; i < 8; i++) {
    float val = analogReadMilliVolts(TEMP_PIN);
    float cc = 2771/val * 0.052+1;     //温度纠正,电阻增大，显示温度降低
    float Drmax = val / vin * REFERENCE_RESISTANCE / (1- val/vin);     //推算电阻
    float Dr = downRESISTANCE *  Drmax / (downRESISTANCE - Drmax )*cc;    //计算出 NTC T温度电阻
    float DTimemax = 1 / (log(Dr / NOMINAL_RESISTANCE) / B_VALUE + 1 / (NOMINAL_TEMPERATURE + 273.15)) - 273.15;//温度换算
    time_arr[i] = DTimemax;
    vm+=time_arr[i];
  }
  Vout = vm/8;

}

void ADC::get()
{
    uint8_t i;
    if (adc_mode_state == channel_low_temp)
    {
        for (i = 0; i < 7; i++)
            adc_buf[i] = adc_buf[i + 1];
        adc_buf[i] = analogRead(0);
    }else{
        for (i = 0; i < 7; i++)
            adc_buf_high[i] = adc_buf_high[i + 1];
        adc_buf_high[i] = analogRead(0);
    }

    set_channel(!adc_mode_state);
    }
    


void ADC::get_voltage()
{
    uint8_t i;
    vol_low = 0;
    for (i = 0; i < 8; i++)
        vol_low += adc_buf[i];
    vol_low >>= 3;
    vol_low = vol_low * 1000 / 4095;

    vol_high = 0;
    for (i = 0; i < 8; i++)
        vol_high += adc_buf_high[i];
    vol_high >>= 3;
    vol_high = vol_high * 1000 / 4095;
}
// 51  150`
void ADC::get_temp_task()
{
    float val = analogReadMilliVolts(TEMP_PIN);

    float cc = 2770/val * 0.046+1;     //温度纠正,电阻增大，显示温度降低
    float Drmax = val / vin * REFERENCE_RESISTANCE / (1- val/vin);     //推算电阻
    float Dr = downRESISTANCE *  Drmax / (downRESISTANCE - Drmax )*cc;    //计算出 NTC T温度电阻
    float DTimemax = 1 / (log(Dr / NOMINAL_RESISTANCE) / B_VALUE + 1 / (NOMINAL_TEMPERATURE + 273.15)) - 273.15;//温度换算

    if ( Vout - DTimemax > -temp_w && temp_w > Vout - DTimemax) {
     vm += DTimemax-time_arr[i_forV];
     time_arr[i_forV] = DTimemax;
     i_forV++;
     if(i_forV == 8){
        i_forV = 0;
        }
     Vout = vm/8;
     }
    now_temp = Vout;


    /*
    const double celsius = thermistor->readCelsius();

    if (item/10-celsius > -temp_w && temp_w > item/10-celsius) {
        item = item - time_arr[0];
        for (int i = 0; i < 9; i++) {
            time_arr[i] = time_arr[i + 1];
        }
        time_arr[9] = celsius;
        item += time_arr[9];
        //Serial.println(item/10);
    }


    ttmep = item/10;//
    now_temp = ttmep+100;//
    /*if(ttmep<70){
        //ttmep += 30;
        now_temp = ((Parallel_Temp-ttmep)/Parallel_Temp*0.6+1)*ttmep;
    }else{
        now_temp = ((Parallel_Temp-ttmep)/Parallel_Temp*0.06+1)*ttmep;
    }*/

}

void ADC::set_channel(bool channel)
{
    digitalWrite(switch_io, channel);
    adc_mode_state = channel;
}

static uint16_t last_temp = 0;

void adc_max_temp_auto_feed()
{
    /*if (last_temp != adc.now_temp)
    {
        last_temp = adc.now_temp;
    }
    else
    {
        adc.adc_max_temp_auto_flg = 1;
        if (adc.now_temp != 38)
        {
            adc.adc_max_temp = adc.now_temp;
            ec11.int_close();
            EEPROM.begin(eeprom_size);
            EEPROM.write(adc_max_temp_low_add, adc.adc_max_temp & 0xff);
            EEPROM.write(adc_max_temp_high_add, adc.adc_max_temp >> 8);
            EEPROM.commit();
            EEPROM.end();
            ec11.int_work();
        }
        adc_max_temp_tic.detach();
    }*/
}

bool ADC::adc_max_temp_auto()
{
    return adc_max_temp_auto_flg;
}
