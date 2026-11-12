#ifndef CALIB_CONTROLLER_H
#define CALIB_CONTROLLER_H

#include <cmath>
#include <cstdint>

inline float calibration_target(uint8_t point)
{
    static constexpr float targets[] = {0.0f, 100.0f, 160.0f, 200.0f};
    return point < 4 ? targets[point] : 0.0f;
}

inline bool calibration_near_target(uint8_t point, float raw, float instant)
{
    if (point >= 4 || !std::isfinite(raw) || !std::isfinite(instant)) return false;
    if (point == 0) return true;
    const float target = calibration_target(point);
    return std::fabs(raw - target) <= 2.0f && std::fabs(instant - target) <= 2.0f;
}

class CalibPid {
public:
    void reset()
    {
        integral_ = derivative_ = temperature_ = 0.0f;
        sampled_at_ = sequence_ = 0;
        duty_ = 0;
        have_sample_ = false;
    }

    uint8_t update(float target, float temperature, uint32_t sampled_at,
                   uint32_t sequence, uint8_t limit)
    {
        const float error = target - temperature;
        const float proportional = 4.0f * error;
        if (!std::isfinite(target) || !std::isfinite(temperature) ||
            !std::isfinite(proportional) || target <= 0.0f || limit == 0) {
            reset();
            return 0;
        }

        // 降低上限立即约束积分和缓存输出，重复快照也不能越过新预算。
        integral_ = clamp(integral_, limit);
        if (duty_ > limit) duty_ = limit;
        if (have_sample_ && sequence == sequence_) return duty_;

        if (have_sample_) {
            const uint32_t elapsed = sampled_at - sampled_at_;
            if (elapsed > 2500) {
                // 超期样本只重建基线；同一序列后续回调仍保持零输出。
                reset();
                remember(temperature, sampled_at, sequence);
                return 0;
            }
            if (elapsed != 0) {
                const float dt = elapsed * 0.001f;
                const float rate = (temperature - temperature_) / dt;
                const float filtered = derivative_ + dt / (4.0f + dt) * (rate - derivative_);
                const float accumulated = integral_ + 0.04f * error * dt;
                if (!std::isfinite(filtered) || !std::isfinite(accumulated)) {
                    reset();
                    return 0;
                }
                derivative_ = filtered;
                const float candidate = clamp(accumulated, limit);
                const float trial = proportional + candidate - 12.0f * derivative_;
                if (!std::isfinite(trial)) {
                    reset();
                    return 0;
                }
                // 上饱和时禁止继续累加；反向误差始终可卸载非负积分。
                if (candidate <= integral_ || trial <= limit) integral_ = candidate;
            }
        }

        const float output = proportional + integral_ - 12.0f * derivative_;
        if (!std::isfinite(output)) {
            reset();
            return 0;
        }
        duty_ = static_cast<uint8_t>(clamp(output, limit));
        remember(temperature, sampled_at, sequence);
        return duty_;
    }

private:
    static float clamp(float value, uint8_t limit)
    {
        return value < 0.0f ? 0.0f : (value > limit ? static_cast<float>(limit) : value);
    }

    void remember(float temperature, uint32_t sampled_at, uint32_t sequence)
    {
        temperature_ = temperature;
        sampled_at_ = sampled_at;
        sequence_ = sequence;
        have_sample_ = true;
    }

    float integral_ = 0.0f;
    float derivative_ = 0.0f;
    float temperature_ = 0.0f;
    uint32_t sampled_at_ = 0;
    uint32_t sequence_ = 0;
    uint8_t duty_ = 0;
    bool have_sample_ = false;
};

#endif
