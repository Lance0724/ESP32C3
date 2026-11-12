#include "PWM.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cmath>

namespace {
StaticSemaphore_t output_mutex_storage;
SemaphoreHandle_t output_mutex;
portMUX_TYPE calib_mux = portMUX_INITIALIZER_UNLOCKED;

class OutputGuard {
public:
    explicit OutputGuard(TickType_t wait = portMAX_DELAY)
        : held_(xSemaphoreTake(output_mutex, wait) == pdTRUE) {}
    ~OutputGuard() { if (held_) xSemaphoreGive(output_mutex); }
    explicit operator bool() const { return held_; }
private:
    bool held_;
};

uint8_t calibration_limit(uint8_t bed_limit)
{
    const float budget = fminf(1.0f, POWER_SUPPLY * BED_RESISTOR / (VIN * VIN));
    const uint8_t supply_limit = static_cast<uint8_t>(std::floor(budget * 255.0f));
    return bed_limit < supply_limit ? bed_limit : supply_limit;
}
}


Ticker pwm_tic;
PWM pwm;
uint8_t count = 0;

int16_t ek[3] = {0, 0, 0};

PWM::PWM()
{
    output_mutex = xSemaphoreCreateMutexStatic(&output_mutex_storage);
    //pinMode(HEB_PIN, OUTPUT);
    //pinMode(FAN_PIN, OUTPUT);
    pinMode(LEDP_IN, OUTPUT);
    analogWrite(HEB_PIN, 0);
    analogWrite(FAN_PIN, 0);
    digitalWrite(LEDP_IN, LOW);

}

void pwm_irq()
{
    // 输出互斥锁不阻塞共享定时线程，防止旧普通回调越过停热重新输出。
    OutputGuard output(0);
    if (!output) return;
    if (pwm.calibration_locked()) {
        pwm.calibration_tick();
        return;
    }
    count++;
    if (!count && pwm.high_time){
        analogWrite(HEB_PIN, pwm.BED_PWM_PUT);
        }
    if (count > pwm.high_time){
        analogWrite(HEB_PIN, 0);
        }
    float POWER = VIN * VIN / (adc.now_temp/100.0+BED_RESISTOR);
    float POWER_OUT = POWER_SUPPLY / POWER * 250;
    if (POWER_OUT > 253)
    {
        pwm.BED_PWM_PUT = 253;
    }else{
        pwm.BED_PWM_PUT = POWER_OUT;
    }
}

void PWM::begin()
{
    OutputGuard output;
    if (calibration_locked()) return;
    if (temp_mode == Co_Temp)
        temp_time_buf = temp_mode1_time;
    else
    {
        if (adc.now_temp > curve_temp_buf[0])
            return;
        backflow_working_state = 1;
        backflow_temp_tmp = adc.now_temp;
        percent = 0;
    }

    //pinMode(HEB_PIN, OUTPUT);
    analogWrite(HEB_PIN, 0);
    pwm_tic.attach_ms(4, pwm_irq);
    power = 1;

    ui.heat_flg = 1;
    oled_sleep_t = 0;
}
void PWM::end()
{
    OutputGuard output;
    if (calibration_locked()) {
        request_calibration_stop(CalibStop::Cancelled);
        return;
    }
    pwm_tic.detach();
    high_time = 0;
    analogWrite(HEB_PIN, 0);
    power = 0;

    backflow_working_state = 0;
    if (temp_mode == Co_Temp && temp_reached_flg)
        ui.temp_time_switch_flg = true;
    ui.heat_flg = 1;
    oled_sleep_t = 0;
}

void PWM::temp_set()
{
    if (!pwm.power)
    {
        high_time = 0;
        return;
    }

    if (temp_mode == Co_Temp)
    {
        need_set_temp = temp_buf;
        if (adc.now_temp >= temp_buf && temp_reached_flg == false)
        {
            if (ui.temp_time_switch_flg == false)
                ui.temp_time_switch_flg = true;
        }
        if (temp_reached_flg == true && temp_time_buf == 0)
        {
            end();

            return;
        }
    }
    else
        need_set_temp = backflow_temp_tmp;

    ////////////////////////////////////////////

    ek[2] = ek[1];
    ek[1] = ek[0];
    ek[0] = need_set_temp - adc.now_temp;

    //if (adc.now_temp == 38) {
        //pwm_buf = need_set_temp;
    //} else {
            pwm_buf_f += ek[0];
        //if (adc.now_temp < 180){
            if (pwm_buf_f > 20)
                pwm_buf_f = 20;
            else if (pwm_buf_f < -20)
                pwm_buf_f = -20;
        /*}else{
            if (pwm_buf_f > 50)
                pwm_buf_f = 50;
            else if (pwm_buf_f < -50)
                pwm_buf_f = -50;
        }*/
        //if (adc.now_temp < 180)
        pwm_buf = kp * (ek[0] + ki * pwm_buf_f + kd * (ek[0] - ek[1]));
        //else
        //    pwm_buf = kp * (ek[0] + ki_high * pwm_buf_f + kd * (ek[0] - ek[1]));
        if (ek[0] < 10 && ek[0] > 3){
            if (adc.now_temp < 200)
                pwm_buf /= 3;
        } else if (ek[0] < 4){
            if (adc.now_temp < 200)
                pwm_buf /= 8;
            else 
                pwm_buf /= 3;
            if (ek[0] > -1)
                pwm_buf += need_set_temp / 10;
        }
        if (ek[0] < 0 && pwm_buf > 0){
            pwm_buf = 0;
        }
    //}

    if (pwm_buf < 0){
        pwm_buf = 0;
    }
    if (pwm_buf > 255){
        pwm_buf = 255;
    }
    high_time = pwm_buf;
    // high_time = 255;
    Serial.println(high_time);
}




//---------fan 控制---------
void PWM::fan()
{
    OutputGuard output;
    if (!calibration_locked()) fan_state = !fan_state;
}

void PWM::fan_time()
{
    OutputGuard output;
    if (calibration_locked()) return;
    if (adc.now_temp > 50) {
        analogWrite(FAN_PIN, fan_state ? 200 : 84);
    } else {
        analogWrite(FAN_PIN, 0);
        fan_state = 0;
    }
}

CalibStatus PWM::calibration_status() const
{
    portENTER_CRITICAL(&calib_mux);
    const CalibStatus status = calib_status_;
    portEXIT_CRITICAL(&calib_mux);
    return status;
}

bool PWM::calibration_locked() const
{
    const CalibStatus status = calibration_status();
    return status.active || status.cooling;
}

bool PWM::calibration_active() const
{
    return calibration_status().active;
}

bool PWM::calibration_can_save() const
{
    const CalibStatus status = calibration_status();
    return status.stopped && !status.active && status.duty == 0 &&
        status.reason == CalibStop::Completed &&
        calib_stop_.load(std::memory_order_acquire) ==
            static_cast<uint32_t>(CalibStop::Completed);
}

void PWM::request_calibration_stop(CalibStop reason)
{
    if (reason == CalibStop::None) return;
    uint32_t previous = calib_stop_.load(std::memory_order_relaxed);
    const uint32_t requested = static_cast<uint32_t>(reason);
    // 完成不能覆盖取消/故障；完成请求在实际提交前仍可被终止抢占。
    while (previous == 0 || previous == static_cast<uint32_t>(CalibStop::Completed)) {
        if (calib_stop_.compare_exchange_weak(previous, requested,
                std::memory_order_release, std::memory_order_relaxed)) break;
    }
}

bool PWM::start_calibration()
{
    OutputGuard output;
    const TempSample sample = adc.snapshot();
    if (power || backflow_working_state != 0 || calibration_locked() ||
        !sample.ready || !sample.healthy ||
        static_cast<uint32_t>(millis() - sample.sampled_at) > 1500 ||
        sample.raw_temp < 10 || sample.raw_temp > 40 ||
        sample.instant_temp < 10 || sample.instant_temp > 40) return false;

    pwm_tic.detach();
    analogWrite(HEB_PIN, 0);
    analogWrite(FAN_PIN, 0);
    high_time = 0;
    count = 0;
    power = 0;
    backflow_working_state = 0;
    temp_reached_flg = false;
    temp_time_buf = 0;
    min_count = 0;
    ui.temp_time_switch_flg = false;
    pwm_buf_f = 0;
    ek[0] = ek[1] = ek[2] = 0;
    calib_pid_.reset();
    const uint32_t now = millis();
    calib_stop_.store(0, std::memory_order_release);
    portENTER_CRITICAL(&calib_mux);
    calib_status_ = CalibStatus{};
    calib_status_.active = true;
    calib_heartbeat_at_ = calib_point_at_ = calib_session_at_ = now;
    calib_cool_tracking_ = false;
    portEXIT_CRITICAL(&calib_mux);
    pwm_tic.attach_ms(4, pwm_irq);
    return true;
}

bool PWM::set_calibration_point(uint8_t point)
{
    if (point >= CALIB_POINT_NUM) return false;
    OutputGuard output;
    portENTER_CRITICAL(&calib_mux);
    const bool allowed = calib_status_.active &&
        point == static_cast<uint8_t>(calib_status_.point + 1) &&
        calib_stop_.load(std::memory_order_acquire) == 0;
    if (allowed) {
        calib_status_.point = point;
        calib_status_.duty = 0;
        calib_point_at_ = millis();
    }
    portEXIT_CRITICAL(&calib_mux);
    if (allowed) {
        calib_pid_.reset();
        analogWrite(HEB_PIN, 0);
    }
    return allowed;
}

void PWM::calibration_heartbeat()
{
    portENTER_CRITICAL(&calib_mux);
    if (calib_status_.active) calib_heartbeat_at_ = millis();
    portEXIT_CRITICAL(&calib_mux);
}

void PWM::calibration_tick()
{
    // 仅由持有输出互斥锁的PWM回调调用；临界区内不执行外设操作。
    CalibStatus status = calibration_status();
    const uint32_t now = millis();
    const TempSample sample = adc.snapshot();
    if (status.active) {
        uint32_t heartbeat_at, point_at, session_at;
        portENTER_CRITICAL(&calib_mux);
        heartbeat_at = calib_heartbeat_at_;
        point_at = calib_point_at_;
        session_at = calib_session_at_;
        portEXIT_CRITICAL(&calib_mux);

        CalibStop fault = CalibStop::None;
        if (!sample.ready || !sample.healthy) fault = CalibStop::Sensor;
        else if (static_cast<uint32_t>(now - sample.sampled_at) > 2500)
            fault = CalibStop::SampleTimeout;
        else if (!std::isfinite(sample.instant_temp) || !std::isfinite(sample.raw_temp) ||
                 !std::isfinite(sample.calibrated_temp))
            fault = CalibStop::Sensor;
        else if (sample.instant_temp >= 230 || sample.raw_temp >= 230 ||
                 sample.calibrated_temp >= 230)
            fault = CalibStop::OverTemp;
        else if (static_cast<uint32_t>(now - heartbeat_at) > 250)
            fault = CalibStop::HeartbeatTimeout;
        else if (static_cast<uint32_t>(now - session_at) >= 1800000)
            fault = CalibStop::SessionTimeout;
        else if (static_cast<uint32_t>(now - point_at) >= 600000)
            fault = CalibStop::PointTimeout;
        if (fault != CalibStop::None) request_calibration_stop(fault);

        const CalibStop stop = static_cast<CalibStop>(
            calib_stop_.load(std::memory_order_acquire));
        if (stop != CalibStop::None) {
            calib_pid_.reset();
            analogWrite(HEB_PIN, 0);
            analogWrite(FAN_PIN, FAN_PWM);
            status.active = false;
            status.cooling = true;
            status.stopped = true;
            status.duty = 0;
            status.reason = stop;
            calib_cool_tracking_ = false;
        } else {
            const uint8_t duty = calib_pid_.update(calibration_target(status.point),
                sample.instant_temp, sample.sampled_at, sample.sequence,
                calibration_limit(BED_PWM_PUT));
            if (duty != status.duty) analogWrite(HEB_PIN, duty);
            status.duty = duty;
        }
    } else if (status.cooling) {
        const CalibStop stop = static_cast<CalibStop>(
            calib_stop_.load(std::memory_order_acquire));
        if (status.reason == CalibStop::Completed && stop != CalibStop::None)
            status.reason = stop;
        const bool cool = sample.ready && sample.healthy &&
            static_cast<uint32_t>(now - sample.sampled_at) <= 2500 &&
            std::isfinite(sample.raw_temp) && std::isfinite(sample.calibrated_temp) &&
            sample.raw_temp < 50 && sample.calibrated_temp < 50;
        if (!cool) {
            calib_cool_tracking_ = false;
        } else if (!calib_cool_tracking_) {
            calib_cool_at_ = now;
            calib_cool_tracking_ = true;
        } else if (static_cast<uint32_t>(now - calib_cool_at_) >= 10000) {
            analogWrite(FAN_PIN, 0);
            fan_state = 0;
            status.cooling = false;
            pwm_tic.detach();
        }
    }
    portENTER_CRITICAL(&calib_mux);
    calib_status_ = status;
    portEXIT_CRITICAL(&calib_mux);
}

