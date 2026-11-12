#include "ADC.h"

#include <cmath>
#include <limits>

namespace
{
constexpr double VIN_MV = 3300.0;
constexpr double DOWN_RESISTANCE = 20000.0;
constexpr double REFERENCE_RESISTANCE = 5100.0;
constexpr double NOMINAL_RESISTANCE = 100000.0;
constexpr double NOMINAL_KELVIN = 25.0 + 273.15;
constexpr double B_VALUE = 3950.0;
constexpr float SAMPLE_TOLERANCE = 200.0f;
constexpr uint8_t WINDOW_SIZE = 8;

bool temperature_in_range(float value)
{
    return std::isfinite(value) && value >= 0.0f && value <= 400.0f;
}

bool read_temperature(float &temperature)
{
    const double millivolts = analogReadMilliVolts(TEMP_PIN);
    if (!std::isfinite(millivolts) || millivolts <= 0.0 || millivolts >= VIN_MV)
        return false;

    const double divider = 1.0 - millivolts / VIN_MV;
    if (!std::isfinite(divider) || divider <= 0.0)
        return false;
    const double parallel_resistance = millivolts / VIN_MV * REFERENCE_RESISTANCE / divider;
    const double resistance_denominator = DOWN_RESISTANCE - parallel_resistance;
    if (!std::isfinite(parallel_resistance) || parallel_resistance <= 0.0 ||
        !std::isfinite(resistance_denominator) || resistance_denominator <= 0.0)
        return false;

    // 启动填充与运行采样统一使用原运行时的经验修正，不重新拟合。
    const double correction = 2770.0 / millivolts * 0.046 + 1.0;
    const double resistance = DOWN_RESISTANCE * parallel_resistance / resistance_denominator * correction;
    const double log_argument = resistance / NOMINAL_RESISTANCE;
    if (!std::isfinite(resistance) || resistance <= 0.0 ||
        !std::isfinite(log_argument) || log_argument <= 0.0)
        return false;

    const double inverse_kelvin = std::log(log_argument) / B_VALUE + 1.0 / NOMINAL_KELVIN;
    if (!std::isfinite(inverse_kelvin) || inverse_kelvin <= 0.0)
        return false;
    temperature = static_cast<float>(1.0 / inverse_kelvin - 273.15);
    return temperature_in_range(temperature);
}
}

ADC adc;

ADC::ADC()
{
    // 不在全局构造阶段访问硬件；既有1Hz任务逐次积累八个有效样本。
}

TempSample ADC::snapshot() const
{
    portENTER_CRITICAL(&sample_mux_);
    const TempSample result = sample_;
    portEXIT_CRITICAL(&sample_mux_);
    return result;
}

void ADC::remap_locked()
{
    const float mapped = calibration_enabled_ ? map_calibrated_temp(calibration_, sample_.raw_temp)
                                               : sample_.raw_temp;
    sample_.calibrated_temp = mapped;
    sample_.healthy = input_healthy_ && temperature_in_range(mapped);
    if (sample_.healthy)
    {
        // 未校准保留截断语义；校准后仅在工程范围检查通过时进行四舍五入。
        now_temp = calibration_enabled_ ? static_cast<uint16_t>(std::lround(mapped))
                                        : static_cast<uint16_t>(mapped);
    }
}

bool ADC::set_calibration(const TempCalibData &data)
{
    if (!validate_temp_calib(data))
        return false;
    portENTER_CRITICAL(&sample_mux_);
    calibration_ = data;
    calibration_enabled_ = true;
    remap_locked();
    portEXIT_CRITICAL(&sample_mux_);
    return true;
}

void ADC::clear_calibration()
{
    portENTER_CRITICAL(&sample_mux_);
    calibration_enabled_ = false;
    calibration_ = TempCalibData{};
    remap_locked();
    portEXIT_CRITICAL(&sample_mux_);
}

bool ADC::has_calibration() const
{
    portENTER_CRITICAL(&sample_mux_);
    const bool enabled = calibration_enabled_;
    portEXIT_CRITICAL(&sample_mux_);
    return enabled;
}

float ADC::calibrate_temp(float raw_t) const
{
    TempCalibData model;
    portENTER_CRITICAL(&sample_mux_);
    const bool enabled = calibration_enabled_;
    if (enabled)
        model = calibration_;
    portEXIT_CRITICAL(&sample_mux_);
    return enabled ? map_calibrated_temp(model, raw_t) : raw_t;
}

void ADC::get_temp_task()
{
    float instant = std::numeric_limits<float>::quiet_NaN();
    const bool valid = read_temperature(instant);
    const uint32_t sampled_at = millis();

    portENTER_CRITICAL(&sample_mux_);
    ++sample_.sequence;
    sample_.instant_temp = instant;
    input_healthy_ = valid;
    if (valid && temp_count_ != 0 &&
        std::fabs(sample_.raw_temp - instant) >= SAMPLE_TOLERANCE)
        input_healthy_ = false;

    if (input_healthy_)
    {
        if (temp_count_ == WINDOW_SIZE)
            temp_sum_ -= temp_window_[temp_index_];
        else
            ++temp_count_;
        temp_window_[temp_index_] = instant;
        temp_sum_ += instant;
        temp_index_ = static_cast<uint8_t>((temp_index_ + 1) % WINDOW_SIZE);
        sample_.raw_temp = static_cast<float>(temp_sum_ / temp_count_);
        sample_.ready = temp_count_ == WINDOW_SIZE;
    }
    remap_locked();
    // 无效尝试仍递增序号，但不能让旧均值变成一份新的健康样本。
    if (sample_.healthy)
        sample_.sampled_at = sampled_at;
    portEXIT_CRITICAL(&sample_mux_);
}

void ADC::get()
{
    uint16_t *buffer = adc_mode_state == channel_low_temp ? adc_buf : adc_buf_high;
    for (uint8_t i = 0; i < WINDOW_SIZE - 1; ++i)
        buffer[i] = buffer[i + 1];
    buffer[WINDOW_SIZE - 1] = analogRead(TEMP_PIN);
    set_channel(!adc_mode_state);
}

void ADC::get_voltage()
{
    uint32_t low_sum = 0;
    uint32_t high_sum = 0;
    for (uint8_t i = 0; i < WINDOW_SIZE; ++i)
    {
        low_sum += adc_buf[i];
        high_sum += adc_buf_high[i];
    }
    vol_low = static_cast<uint16_t>((low_sum / WINDOW_SIZE) * 1000 / 4095);
    vol_high = static_cast<uint16_t>((high_sum / WINDOW_SIZE) * 1000 / 4095);
}

void ADC::set_channel(bool channel)
{
    digitalWrite(switch_io, channel);
    adc_mode_state = channel;
}
