#ifndef _PWM_H_
#define _PWM_H_

#include <Arduino.h>
#include <Ticker.h>
#include <atomic>
#include "ADC.h"
#include "Tick_IRQ.h"


#define VIN 20.0 //电源输入电压
#define POWER_SUPPLY 65 //PD 快充 电源功率W
//9V 24~18W +140C,12V 40~29W +190,15V 66~40 227.
//50-0.2,170-1.6,220-2.2
#define BED_RESISTOR 3.2 //熱床电阻

#define HEB_PIN 2 //加热引脚
#define FAN_PIN 1 //风扇引脚
#define FAN_PWM 255 //0~255风扇功率

#define LEDP_IN 6 //LED




/*#define kp 4.5
#define ki 1.0
#define ki_high 3.0 
#define kd 30.0
*/
#define kp 45.0
#define ki 3.0
#define kd 1.0
//#define ki_high 3.0

#define OFF 0
#define ON 1

#define Re_So 0
#define Co_Temp 1

enum class CalibStop : uint8_t {
    None, Completed, Cancelled, RemoteOff, Sensor, OverTemp,
    SampleTimeout, HeartbeatTimeout, PointTimeout, SessionTimeout
};

struct CalibStatus {
    bool active = false;
    bool cooling = false;
    bool stopped = false;
    uint8_t point = 0;
    uint8_t duty = 0;
    CalibStop reason = CalibStop::None;
};





class PWM{
    public:
    PWM();
    void begin();
    void end();
    void temp_set();
    void fan();
    void fan_time();
    bool start_calibration();
    bool set_calibration_point(uint8_t point);
    void calibration_heartbeat();
    void request_calibration_stop(CalibStop reason);
    CalibStatus calibration_status() const;
    bool calibration_locked() const;
    bool calibration_active() const;
    bool calibration_can_save() const;
    bool power = OFF;
    bool sleep_flg = OFF;
    
    int16_t temp_buf = 0;//设置温度

    uint8_t temp_mode = 0; //0 回流 1：恒温

    int16_t temp_mode1_time = 0;//恒温时间

    uint8_t percent = 0;
    uint8_t BED_PWM_PUT = POWER_SUPPLY / (VIN * VIN / BED_RESISTOR) * 250;//熱床 PWM
    bool temp_reached_flg = false;

    uint8_t backflow_working_state = 0;
    // int16_t backflow_temp_buf = 0;
    int16_t backflow_temp_tmp = 0;

    

    
    private:

    bool fan_state = OFF;

    int16_t need_set_temp = 50;
    
    uint8_t high_time = 0;

    int16_t pwm_buf = 0;
    float pwm_buf_f = 0;
    friend void pwm_irq();
    void calibration_tick();
    CalibStatus calib_status_;
    std::atomic<uint32_t> calib_stop_{0};
    uint32_t calib_heartbeat_at_ = 0;
    uint32_t calib_point_at_ = 0;
    uint32_t calib_session_at_ = 0;
    uint32_t calib_cool_at_ = 0;
    bool calib_cool_tracking_ = false;
    


    
};



extern PWM pwm;



#endif