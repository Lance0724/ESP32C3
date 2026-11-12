#include "CalibController.h"
#include <cassert>
#include <iostream>
#include <limits>

namespace {
void target_checks()
{
    const float expected[] = {0.0f, 100.0f, 160.0f, 200.0f};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (uint8_t point = 0; point < 4; ++point) {
        assert(calibration_target(point) == expected[point]);
        assert(calibration_near_target(point, expected[point], expected[point]));
        assert(!calibration_near_target(point, nan, expected[point]));
        assert(!calibration_near_target(point, expected[point], infinity));
        if (point == 0) continue;
        const float target = expected[point];
        assert(calibration_near_target(point, target - 2.0f, target + 2.0f));
        assert(!calibration_near_target(point, target - 2.1f, target));
        assert(!calibration_near_target(point, target, target + 2.1f));
        assert(!calibration_near_target(point, 25.0f, 25.0f));
    }
    assert(calibration_near_target(0, 25.0f, 40.0f));
    assert(calibration_target(4) == 0.0f && calibration_target(255) == 0.0f);
    assert(!calibration_near_target(4, 0.0f, 0.0f));
    assert(!calibration_near_target(255, 0.0f, 0.0f));
}

void approach_checks()
{
    CalibPid pid;
    constexpr uint8_t limit = 130;
    const uint8_t far = pid.update(100.0f, 25.0f, 0, 0, limit);
    assert(far == limit);
    const float temperatures[] = {60.0f, 80.0f, 90.0f, 95.0f, 99.0f, 100.0f};
    uint8_t near = far;
    for (uint32_t i = 0; i < 6; ++i) {
        near = pid.update(100.0f, temperatures[i], (i + 1) * 1000, i + 1, limit);
        assert(near <= limit);
    }
    assert(near < far);
    assert(pid.update(0.0f, 25.0f, 7000, 7, limit) == 0);

    // 微分只观察测量：同样的温度轨迹不因目标切换产生微分冲击。
    CalibPid changed, fresh;
    assert(changed.update(100.0f, 100.0f, 0, 0, limit) == 0);
    fresh.update(110.0f, 100.0f, 0, 0, limit);
    assert(changed.update(110.0f, 100.0f, 1000, 1, limit) ==
           fresh.update(110.0f, 100.0f, 1000, 1, limit));
}

void sample_timing_checks()
{
    CalibPid reference, repeated, shorter;
    const uint8_t first = reference.update(100.0f, 99.0f, 0, 0, 130);
    assert(repeated.update(100.0f, 99.0f, 0, 0, 130) == first);
    shorter.update(100.0f, 99.0f, 0, 0, 130);
    uint8_t last = first, short_last = first;
    for (uint32_t i = 1; i <= 300; ++i) {
        for (uint32_t tick = 1; tick <= 250; ++tick)
            assert(repeated.update(100.0f, 99.0f, (i - 1) * 1000 + tick * 4,
                                   i - 1, 130) == last);
        last = reference.update(100.0f, 99.0f, i * 1000, i, 130);
        assert(repeated.update(100.0f, 99.0f, i * 1000, i, 130) == last);
        short_last = shorter.update(100.0f, 99.0f, i * 500, i, 130);
    }
    assert(last > first && short_last < last);

    CalibPid same_time;
    const uint8_t initial = same_time.update(100.0f, 99.0f, 1000, 0, 130);
    for (uint32_t i = 1; i <= 1000; ++i)
        assert(same_time.update(100.0f, 99.0f, 1000, i, 130) == initial);
    CalibPid maximum_interval;
    const uint8_t start = maximum_interval.update(100.0f, 99.0f, 0, 0, 130);
    uint8_t after = start;
    for (uint32_t i = 1; i <= 100; ++i)
        after = maximum_interval.update(100.0f, 99.0f, i * 2500, i, 130);
    assert(after > start);
}

void windup_checks()
{
    CalibPid saturated;
    for (uint32_t i = 0; i <= 600; ++i)
        assert(saturated.update(100.0f, 25.0f, i * 1000, i, 100) == 100);
    uint8_t relaxed = 100;
    for (uint32_t i = 601; i <= 661; ++i)
        relaxed = saturated.update(100.0f, 99.0f, i * 1000, i, 100);
    assert(relaxed > 0 && relaxed < 25);

    CalibPid unwind;
    for (uint32_t i = 0; i <= 500; ++i)
        unwind.update(100.0f, 99.0f, i * 1000, i, 130);
    uint8_t holding = 0;
    for (uint32_t i = 501; i <= 560; ++i)
        holding = unwind.update(100.0f, 100.0f, i * 1000, i, 130);
    assert(holding > 0);
    for (uint32_t i = 561; i <= 1160; ++i)
        unwind.update(100.0f, 101.0f, i * 1000, i, 130);
    uint8_t unloaded = holding;
    for (uint32_t i = 1161; i <= 1220; ++i)
        unloaded = unwind.update(100.0f, 100.0f, i * 1000, i, 130);
    assert(unloaded < holding / 2);
}

void cap_and_reset_checks()
{
    CalibPid pid;
    uint8_t accumulated = 0;
    for (uint32_t i = 0; i <= 200; ++i)
        accumulated = pid.update(100.0f, 95.0f, i * 1000, i, 130);
    assert(accumulated > 7);
    assert(pid.update(100.0f, 95.0f, 200000, 200, 7) == 7);
    assert(pid.update(100.0f, 95.0f, 200000, 200, 130) == 7);
    uint8_t settled = 130;
    for (uint32_t i = 201; i <= 260; ++i)
        settled = pid.update(100.0f, 100.0f, i * 1000, i, 130);
    assert(settled <= 7);
    assert(pid.update(100.0f, 25.0f, 261000, 261, 3) <= 3);

    pid.reset();
    CalibPid fresh;
    assert(pid.update(100.0f, 99.0f, 261000, 261, 130) ==
           fresh.update(100.0f, 99.0f, 261000, 261, 130));
    assert(pid.update(100.0f, 25.0f, 262000, 262, 0) == 0);
    assert(pid.update(0.0f, 25.0f, 263000, 263, 130) == 0);
}

void rollover_and_invalid_checks()
{
    CalibPid wrapped, reference;
    const uint32_t origin = UINT32_MAX - 500;
    for (uint32_t i = 0; i <= 120; ++i) {
        const uint8_t expected = reference.update(100.0f, 99.0f, i * 1000, i, 130);
        assert(wrapped.update(100.0f, 99.0f, origin + i * 1000,
                              UINT32_MAX + i, 130) == expected);
    }

    CalibPid gap;
    assert(gap.update(100.0f, 25.0f, 0, 0, 130) > 0);
    assert(gap.update(100.0f, 25.0f, 2501, 1, 130) == 0);
    assert(gap.update(100.0f, 25.0f, 2501, 1, 130) == 0);
    assert(gap.update(100.0f, 25.0f, 3501, 2, 130) > 0);
    assert(gap.update(100.0f, 25.0f, 3500, 3, 130) == 0);
    assert(gap.update(100.0f, 25.0f, 3500, 3, 130) == 0);

    const float invalid[] = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()
    };
    for (float value : invalid) {
        CalibPid pid;
        pid.update(100.0f, 25.0f, 0, 0, 130);
        assert(pid.update(value, 25.0f, 1000, 1, 130) == 0);
        CalibPid fresh;
        assert(pid.update(100.0f, 99.0f, 2000, 2, 130) ==
               fresh.update(100.0f, 99.0f, 2000, 2, 130));
        assert(pid.update(100.0f, value, 2000, 2, 130) == 0);
    }
    CalibPid disabled;
    assert(disabled.update(-1.0f, 25.0f, 0, 0, 130) == 0);
}
}

void controller_checks()
{
    target_checks();
    approach_checks();
    sample_timing_checks();
    windup_checks();
    cap_and_reset_checks();
    rollover_and_invalid_checks();
    std::cout << "PASS calibration controller: targets, bounded PID, sample timing, windup, caps, reset, rollover, invalid input\n";
}
