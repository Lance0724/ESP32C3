#ifndef _ADC_H_
#define _ADC_H_

#define TEMP_PIN 0
#define switch_io 7
#define channel_low_temp 0
#define channel_high_temp 1

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include "TempCalibration.h"

struct TempSample
{
    float raw_temp = 0;
    float instant_temp = 0;
    float calibrated_temp = 0;
    uint32_t sampled_at = 0;
    uint32_t sequence = 0;
    bool healthy = false;
    bool ready = false;
};

class ADC
{
public:
    ADC();
    void get_voltage();
    void get_temp_task();
    void get();
    void set_channel(bool channel);

    TempSample snapshot() const;
    bool set_calibration(const TempCalibData &data);
    void clear_calibration();
    bool has_calibration() const;
    float calibrate_temp(float raw_t) const;

    uint16_t now_temp = 0;
    uint16_t now_temp_high = 0;
    bool adc_get_temp_flg = false;
    uint16_t hotbed_max_temp = 0;
    uint16_t adc_max_temp = 0;

private:
    // 仅短时保护快照、滤波状态和整份模型，硬件采样及对数计算在锁外完成。
    mutable portMUX_TYPE sample_mux_ = portMUX_INITIALIZER_UNLOCKED;
    TempSample sample_;
    TempCalibData calibration_;
    bool calibration_enabled_ = false;
    bool input_healthy_ = false;
    float temp_window_[8] = {};
    double temp_sum_ = 0;
    uint8_t temp_count_ = 0;
    uint8_t temp_index_ = 0;

    uint16_t adc_buf[8] = {};
    uint16_t adc_buf_high[8] = {};
    uint16_t vol_low = 0;
    uint16_t vol_high = 0;
    bool adc_mode_state = channel_low_temp;

    void remap_locked();
};

extern ADC adc;

#endif
