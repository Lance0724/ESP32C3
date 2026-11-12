#ifndef CALIB_STABILITY_H
#define CALIB_STABILITY_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

class CalibStability
{
public:
    static constexpr uint32_t window_ms = 30000;
    static constexpr uint32_t max_gap_ms = 1500;
    static constexpr size_t capacity = 64;

    void reset()
    {
        head_ = 0;
        count_ = 0;
        seen_ = false;
    }

    void observe(float raw, float instant, uint32_t sampled_at, uint32_t sequence,
                 bool healthy, bool ready, uint32_t now)
    {
        if (!healthy || !ready || !isfinite(raw) || !isfinite(instant) ||
            static_cast<uint32_t>(now - sampled_at) > max_gap_ms)
        {
            reset();
            return;
        }
        if (seen_ && sequence == sequence_)
            return;
        const bool contiguous = !seen_ || static_cast<uint32_t>(sequence - sequence_) == 1;
        seen_ = true;
        sequence_ = sequence;
        if (count_ && (!contiguous ||
                       static_cast<uint32_t>(sampled_at - at(count_ - 1).time) > max_gap_ms ||
                       sampled_at == at(count_ - 1).time))
        {
            head_ = 0;
            count_ = 0;
        }
        // 保留覆盖窗口左边界的一个样本，不能用缓存重复读取凑满三十秒。
        while (count_ > 1 && static_cast<uint32_t>(sampled_at - at(1).time) >= window_ms)
            pop();
        if (count_ == capacity)
            pop();
        samples_[(head_ + count_) % capacity] = {raw, instant, sampled_at};
        ++count_;
    }

    bool stable(uint32_t now) const
    {
        if (count_ < 2 || static_cast<uint32_t>(now - at(count_ - 1).time) > max_gap_ms ||
            span_ms() < window_ms)
            return false;
        float raw_min = at(0).raw, raw_max = raw_min;
        float instant_min = at(0).instant, instant_max = instant_min;
        for (size_t i = 1; i < count_; ++i)
        {
            const Sample &sample = at(i);
            if (sample.raw < raw_min) raw_min = sample.raw;
            if (sample.raw > raw_max) raw_max = sample.raw;
            if (sample.instant < instant_min) instant_min = sample.instant;
            if (sample.instant > instant_max) instant_max = sample.instant;
        }
        return raw_max - raw_min <= 1.0f && instant_max - instant_min <= 2.0f;
    }

    float mean() const
    {
        if (!count_) return 0.0f;
        float total = 0.0f;
        for (size_t i = 0; i < count_; ++i)
            total += at(i).raw;
        return total / static_cast<float>(count_);
    }

    uint32_t span_ms() const
    {
        return count_ > 1 ? static_cast<uint32_t>(at(count_ - 1).time - at(0).time) : 0;
    }

private:
    struct Sample { float raw; float instant; uint32_t time; };
    const Sample &at(size_t index) const { return samples_[(head_ + index) % capacity]; }
    void pop() { head_ = (head_ + 1) % capacity; --count_; }
    Sample samples_[capacity] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t sequence_ = 0;
    bool seen_ = false;
};

#endif
