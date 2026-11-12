#include "Harness.h"
#include "Blinker.h"
#include "CalibController.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

extern void pwm_irq();
extern int8_t page_num, page2_menu_num;
extern void button1_callback(const String&);
extern void button2_callback(const String&);
extern void button3_callback(const String&);
extern void button4_callback(const String&);
extern void button5_callback(const String&);
extern void slider1_callback(int32_t);
extern void miotPowerState(const String&);
extern void miotColor(int32_t);
extern void miotMode(uint8_t);
extern void oled_checks();
extern void controller_checks();
extern void assert_oled_four_rows();
extern void save_oled_frame(const char *name);
extern void loop();
extern unsigned char Oled_GRAM[PAGE_SIZE][WIDTH];

static void close_to(float actual, float expected) {
    assert(std::isfinite(actual) && std::fabs(actual - expected) < 0.001f);
}
static TempCalibData example() {
    TempCalibData data;
    const float raw[] = {25, 100, 160, 220};
    const uint16_t real[] = {25, 105, 165, 225};
    std::copy(raw, raw + 4, data.raw_temp);
    std::copy(real, real + 4, data.real_temp);
    return data;
}
static void model_checks() {
    TempCalibData data = example();
    assert(validate_temp_calib(data));
    const float x[] = {25, 100, 160, 220, 62.5f, 130, 190, 10, 240};
    const float y[] = {25, 105, 165, 225, 65, 135, 195, 9, 245};
    for (size_t i = 0; i < 9; ++i) close_to(map_calibrated_temp(data, x[i]), y[i]);
    auto invalid = data;
    invalid.raw_temp[1] = 25; assert(!validate_temp_calib(invalid));
    invalid = data; invalid.raw_temp[2] = 80; assert(!validate_temp_calib(invalid));
    invalid = data; invalid.raw_temp[1] = 34.9f; assert(!validate_temp_calib(invalid));
    invalid = data; invalid.real_temp[1] = 29; assert(!validate_temp_calib(invalid));
    invalid = data; invalid.raw_temp[0] = std::numeric_limits<float>::quiet_NaN();
    assert(!validate_temp_calib(invalid));
    invalid = data; invalid.raw_temp[3] = std::numeric_limits<float>::infinity();
    assert(!validate_temp_calib(invalid));
    invalid = data; invalid.real_temp[3] = 231; assert(!validate_temp_calib(invalid));
    invalid = data; invalid.raw_temp[1] = 35; assert(!validate_temp_calib(invalid, 2));
    assert(!validate_temp_calib(data, 0) && !validate_temp_calib(data, 5));
    invalid = data; invalid.raw_temp[0] = 25.1f; invalid.raw_temp[1] = 35.1f;
    invalid.real_temp[1] = 35;
    assert(validate_temp_calib(invalid));
    uint8_t encoded[CALIB_RECORD_SIZE];
    assert(encode_temp_calib(invalid, encoded));
    TempCalibData decoded;
    assert(decode_temp_calib(encoded, decoded));
    close_to(map_calibrated_temp(decoded, 30.1f), 30);
    assert(encode_temp_calib(data, encoded));
    for (size_t byte = 0; byte < CALIB_RECORD_SIZE; ++byte) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            encoded[byte] ^= static_cast<uint8_t>(1u << bit);
            decoded = data;
            assert(!decode_temp_calib(encoded, decoded));
            close_to(decoded.raw_temp[1], 100);
            encoded[byte] ^= static_cast<uint8_t>(1u << bit);
        }
    }
    assert(decode_temp_calib(encoded, decoded));
    close_to(map_calibrated_temp(decoded, 130), 135);
    std::cout << "PASS model: interpolation, extrapolation, geometry, quantization, 176 corruptions\n";
}

static void sample(ADC &target, uint32_t mv) {
    host_adc_mv = mv;
    host_now += 1000;
    target.get_temp_task();
}
static void settle(ADC &target, uint32_t mv) {
    for (unsigned i = 0; i < 8; ++i) sample(target, mv);
}
// 用真实ADC实现找激励电压，不复制生产换算公式。
static uint32_t voltage_for(float temperature) {
    ADC probe;
    uint32_t low = 100, high = 2600, best = 2500;
    float error = 10000;
    for (unsigned i = 0; i < 12; ++i) {
        const uint32_t mv = (low + high) / 2;
        settle(probe, mv);
        const TempSample observed = probe.snapshot();
        assert(observed.healthy);
        const float difference = std::fabs(observed.raw_temp - temperature);
        if (difference < error) { error = difference; best = mv; }
        if (observed.raw_temp > temperature) low = mv + 1;
        else high = mv - 1;
    }
    assert(error < 0.5f);
    return best;
}
static uint32_t room_mv, warm_mv, mid_mv, high_mv, trip_mv;
static uint32_t plateau_mv, near_edge_mv, outside_mv;
static void adc_checks() {
    ADC local;
    assert(!local.snapshot().ready);
    for (unsigned i = 0; i < 7; ++i) sample(local, room_mv);
    assert(local.snapshot().healthy && !local.snapshot().ready);
    sample(local, room_mv);
    const auto good = local.snapshot();
    assert(good.ready && good.healthy);
    assert(local.now_temp == static_cast<uint16_t>(good.raw_temp));
    sample(local, 0);
    auto bad = local.snapshot();
    assert(!bad.healthy && bad.sampled_at == good.sampled_at && bad.sequence == good.sequence + 1);
    assert(local.now_temp == static_cast<uint16_t>(good.raw_temp));
    sample(local, 3300); assert(!local.snapshot().healthy);
    sample(local, 2700); assert(!local.snapshot().healthy);
    sample(local, room_mv); assert(local.snapshot().healthy);
    assert(local.set_calibration(example()));
    const auto corrected = local.snapshot();
    close_to(corrected.calibrated_temp, map_calibrated_temp(example(), corrected.raw_temp));
    assert(local.now_temp == static_cast<uint16_t>(std::lround(corrected.calibrated_temp)));
    auto invalid = example(); invalid.raw_temp[2] = 100;
    assert(!local.set_calibration(invalid));
    close_to(local.calibrate_temp(130), 135);
    // 外推越界时保持上一有效整数，不允许回绕。
    auto excessive = example();
    excessive.raw_temp[1] = 60; excessive.raw_temp[2] = 100; excessive.raw_temp[3] = 140;
    excessive.real_temp[1] = 60; excessive.real_temp[2] = 100; excessive.real_temp[3] = 230;
    assert(local.set_calibration(excessive));
    settle(local, warm_mv);
    uint16_t previous = local.now_temp;
    for (unsigned i = 0; i < 8; ++i) {
        sample(local, trip_mv);
        if (local.snapshot().calibrated_temp > 400)
            assert(!local.snapshot().healthy && local.now_temp == previous);
        else
            previous = local.now_temp;
    }
    assert(local.snapshot().calibrated_temp > 400 && !local.snapshot().healthy);
    local.clear_calibration();
    assert(!local.has_calibration());
    std::cout << "PASS ADC: readiness, invalid domains, freshness, atomic model publication\n";
}
static void storage_checks() {
    adc.clear_calibration();
    settle(adc, room_mv);
    EEPROM.flash.fill(0);
    pwm.temp_buf = 173;
    pwm.temp_mode = 1;
    adc.hotbed_max_temp = adc.adc_max_temp = 255;
    eeprom.write_flg = 2;
    eeprom.write_task();
    eeprom.write_str(blinker_id_add, "abcdefghijkl");
    assert(eeprom.save_calib_data(example()));
    const auto saved_flash = EEPROM.flash;
    adc.clear_calibration();
    eeprom.read_all_data();
    assert(adc.has_calibration() && pwm.temp_buf == 173);
    close_to(adc.calibrate_temp(130), 135);
    assert(wifima.blinker_id == "abcdefghijkl");
    auto changed = example(); changed.real_temp[1] = 108;
    EEPROM.fail_begin = true;
    assert(!eeprom.save_calib_data(changed));
    EEPROM.fail_begin = false;
    close_to(adc.calibrate_temp(100), 105);
    EEPROM.fail_commit = true;
    assert(!eeprom.save_calib_data(changed));
    EEPROM.fail_commit = false;
    close_to(adc.calibrate_temp(100), 105);
    EEPROM.corrupt_after_commit = true;
    assert(!eeprom.save_calib_data(changed));
    EEPROM.corrupt_after_commit = false;
    close_to(adc.calibrate_temp(100), 105);
    adc.clear_calibration();
    eeprom.read_all_data();
    assert(!adc.has_calibration() && pwm.temp_buf == 173);
    EEPROM.flash = saved_flash;
    eeprom.read_all_data();
    assert(adc.has_calibration());
    eeprom.write_str(blinker_id_add, "abcdefghijklmnop");
    assert(EEPROM.flash == saved_flash);
    assert(EEPROM.begin(eeprom_size));
    eeprom.data_init();
    assert(EEPROM.commit());
    EEPROM.end();
    assert(!adc.has_calibration());
    TempCalibData unused;
    assert(!decode_temp_calib(EEPROM.flash.data() + temp_calib_record_add, unused));
    eeprom.write_flg = 0;
    std::cout << "PASS storage: restart, preserved settings, write faults, readback corruption, reset\n";
}

static void room_and_unlock() {
    if (pwm.calibration_active()) {
        pwm.request_calibration_stop(CalibStop::Cancelled);
        pwm_irq();
    }
    adc.clear_calibration();
    settle(adc, warm_mv);
    settle(adc, room_mv);
    for (unsigned i = 0; i < 12; ++i) {
        sample(adc, room_mv);
        pwm_irq();
    }
    assert(!pwm.calibration_locked());
    pwm.BED_PWM_PUT = 130;
}
static void fresh_now() { adc.get_temp_task(); pwm.calibration_heartbeat(); }
static void stopped(CalibStop expected) {
    const auto status = pwm.calibration_status();
    assert(!status.active && status.cooling && status.stopped && status.reason == expected);
    assert(status.duty == 0 && host_pwm[HEB_PIN] == 0 && host_pwm[FAN_PIN] == FAN_PWM);
    pwm.fan_time(); pwm.fan(); pwm.begin();
    assert(!pwm.power && host_pwm[HEB_PIN] == 0 && host_pwm[FAN_PIN] == FAN_PWM);
}
static void pwm_checks() {
    room_and_unlock();
    assert(pwm.start_calibration());
    pwm_irq(); assert(host_pwm[HEB_PIN] == 0);
    const unsigned writes = EEPROM.writes;
    eeprom.write_flg = 2; eeprom.write_task();
    assert(EEPROM.writes == writes);
    assert(!eeprom.save_calib_data(example()));
    eeprom.write_flg = 0;
    const int budget = static_cast<int>(std::floor(
        std::min(1.0, POWER_SUPPLY * BED_RESISTOR / (VIN * VIN)) * 255.0));
    const int limit = std::min<int>(pwm.BED_PWM_PUT, budget);
    for (uint8_t point = 1; point <= 3; ++point) {
        assert(pwm.set_calibration_point(point));
        pwm_irq();
        assert(host_pwm[HEB_PIN] > 0 && host_pwm[HEB_PIN] <= limit);
        assert(pwm.calibration_status().duty == host_pwm[HEB_PIN]);
        const int unchanged = host_pwm[HEB_PIN];
        pwm_irq();
        assert(host_pwm[HEB_PIN] == unchanged);
        if (point == 1) {
            sample(adc, warm_mv); pwm.calibration_heartbeat(); pwm_irq();
            assert(pwm.calibration_active() && host_pwm[HEB_PIN] < unchanged);
            assert(adc.snapshot().raw_temp < 50 && adc.snapshot().instant_temp > 99);
        }
    }
    const auto old_mode = pwm.temp_mode;
    const auto old_target = pwm.temp_buf;
    button1_callback("on"); button3_callback("on"); button4_callback("on");
    button5_callback("on"); slider1_callback(200); miotColor(255); miotMode(1);
    miotPowerState("on");
    assert(pwm.temp_mode == old_mode && pwm.temp_buf == old_target && !pwm.power);
    button2_callback("off");
    pwm.request_calibration_stop(CalibStop::Completed);
    pwm_irq(); stopped(CalibStop::RemoteOff);
    assert(!pwm.calibration_can_save());

    room_and_unlock(); assert(pwm.start_calibration());
    pwm.request_calibration_stop(CalibStop::Completed);
    assert(!pwm.calibration_can_save());
    pwm.request_calibration_stop(CalibStop::Cancelled);
    pwm_irq(); stopped(CalibStop::Cancelled);
    assert(!pwm.calibration_can_save());

    room_and_unlock(); assert(pwm.start_calibration());
    pwm.request_calibration_stop(CalibStop::Completed);
    pwm_irq(); stopped(CalibStop::Completed);
    assert(pwm.calibration_can_save());
    miotPowerState("off"); pwm_irq();
    assert(!pwm.calibration_can_save());

    room_and_unlock(); assert(pwm.start_calibration());
    host_now += 251; pwm_irq(); stopped(CalibStop::HeartbeatTimeout);
    room_and_unlock(); assert(pwm.start_calibration());
    host_now += 2501; pwm.calibration_heartbeat(); pwm_irq(); stopped(CalibStop::SampleTimeout);
    room_and_unlock(); assert(pwm.start_calibration());
    sample(adc, 0); pwm_irq(); stopped(CalibStop::Sensor);
    room_and_unlock(); assert(pwm.start_calibration());
    settle(adc, warm_mv); pwm.calibration_heartbeat(); pwm_irq();
    sample(adc, trip_mv); pwm.calibration_heartbeat(); pwm_irq(); stopped(CalibStop::OverTemp);
    room_and_unlock(); assert(pwm.start_calibration());
    host_now += 600000; fresh_now(); pwm_irq(); stopped(CalibStop::PointTimeout);
    room_and_unlock(); assert(pwm.start_calibration());
    for (uint8_t point = 1; point <= 3; ++point) {
        host_now += 590000; fresh_now(); assert(pwm.set_calibration_point(point));
    }
    host_now += 30001; fresh_now(); pwm_irq(); stopped(CalibStop::SessionTimeout);
    room_and_unlock();
    host_now = UINT32_MAX - 100;
    fresh_now(); assert(pwm.start_calibration());
    host_now += 200; fresh_now(); pwm_irq(); assert(pwm.calibration_active());
    host_now += 251; pwm_irq(); stopped(CalibStop::HeartbeatTimeout);
    room_and_unlock(); assert(pwm.start_calibration());
    pwm.BED_PWM_PUT = 7;
    assert(pwm.set_calibration_point(1)); pwm_irq(); assert(host_pwm[HEB_PIN] == 7);
    room_and_unlock();
    std::cout << "PASS PWM: budgets, remote exclusion, stop precedence, all deadlines, rollover, cooling\n";
}
static void stability_checks() {
    CalibStability window;
    for (uint32_t i = 0; i < 30; ++i) {
        window.observe(25, 25, i * 1000, i + 1, true, true, i * 1000);
        assert(!window.stable(i * 1000));
    }
    window.observe(25, 25, 30000, 31, true, true, 30000);
    assert(window.stable(30000)); close_to(window.mean(), 25);
    window.observe(25, 25, 31000, 31, true, true, 31000);
    assert(!window.stable(31501));
    window.observe(25, 25, 31501, 32, true, true, 31501);
    assert(!window.stable(31501));
    window.reset();
    for (uint32_t i = 0; i <= 30; ++i)
        window.observe(i == 15 ? 27 : 25, 25, i * 1000, i + 1, true, true, i * 1000);
    assert(!window.stable(30000));
    window.reset();
    for (uint32_t i = 0; i <= 30; ++i)
        window.observe(25, i == 15 ? 28 : 25, i * 1000, i + 1, true, true, i * 1000);
    assert(!window.stable(30000));
    window.reset();
    for (uint32_t i = 0; i <= 30; ++i) {
        const uint32_t now = UINT32_MAX - 10000 + i * 1000;
        window.observe(25, 25, now, UINT32_MAX - 15 + i, true, true, now);
    }
    assert(window.stable(UINT32_MAX - 10000 + 30000u));
    window.observe(25, 25, 21000, 16, false, true, 21000);
    assert(!window.stable(21000));
    std::cout << "PASS stability: full window, cached data, gaps, excursions, clock/sequence rollover\n";
}
static void press(int key) {
    host_now += 1;
    ui_key_callb(ec11_task_is_key, static_cast<int16_t>(key));
    ui.run_task();
}
static void run_ui(uint32_t duration, uint32_t mv) {
    host_adc_mv = mv;
    uint32_t sampled = host_now;
    for (uint32_t elapsed = 0; elapsed < duration; elapsed += 50) {
        host_now += 50;
        if (static_cast<uint32_t>(host_now - sampled) >= 1000) {
            adc.get_temp_task(); sampled = host_now;
        }
        ui.run_task(); pwm_irq();
    }
}
static void assert_oled_row(unsigned row, const char *text) {
    const size_t length = std::strlen(text);
    assert(row < PAGE_SIZE && length <= 21);
    for (size_t i = 0; i < length; ++i)
        assert(text[i] >= ' ' && text[i] <= '~' && text[i] != '%');
    for (unsigned x = 0; x < WIDTH; ++x) {
        const unsigned char expected = x < length * 6 ?
            F6X8[static_cast<unsigned char>(text[x / 6]) - ' '][x % 6] : 0;
        assert(Oled_GRAM[row][x] == expected);
    }
}
static void assert_raw_row() {
    char text[22];
    std::snprintf(text, sizeof(text), "RAW %.1fC UNMAPPED",
                  static_cast<double>(adc.snapshot().raw_temp));
    assert_oled_row(1, text);
}
static void set_reference(unsigned value) {
    const unsigned initial = static_cast<unsigned>(std::lround(adc.snapshot().raw_temp));
    for (unsigned i = std::min(initial, value); i < std::max(initial, value); ++i) {
        ++host_now;
        ui_key_callb(ec11_task_is_encode, value > initial ? 1 : -1);
        ui.run_task();
    }
    char text[22];
    std::snprintf(text, sizeof(text), "REF %uC CLICK: OK", value);
    assert_oled_row(2, text);
}
static void refresh_ui() {
    host_now += 200;
    ui.run_task();
    pwm_irq();
}
static void enter_ui() {
    page_num = 2; page2_menu_num = 7;
    ui.page_switch_flg = false; ui.oled_sleep_flg = false; ui.wake_sleep_change_flg = false;
    press(sw_long);
    assert(page_num == 3 && !pwm.calibration_active());
    assert(host_pwm[HEB_PIN] == 0);
    assert_oled_four_rows();
    save_oled_frame("calibration-confirm");
    press(sw_click);
    assert(pwm.calibration_active() && pwm.calibration_status().point == 0);
    assert_oled_four_rows();
    save_oled_frame("calibration-wait");
}
static void ui_checks() {
    room_and_unlock();
    assert(adc.set_calibration(example()));
    eeprom.write_flg = 0;
    ec11.speed_up(true); ec11.double_click(true);
    enter_ui();
    run_ui(32500, room_mv);
    assert(host_pwm[HEB_PIN] == 0);
    assert_oled_four_rows();
    assert_oled_row(0, "P0 ROOM / NO HEAT");
    assert_raw_row();
    assert_oled_row(3, "HOLD: LOCAL STOP");
    save_oled_frame("calibration-input");
    // 错误不能遮住外部实测值及本地停止提示。
    set_reference(41); press(sw_click);
    assert_oled_row(0, "P0 ROOM REF CHECK");
    assert_oled_row(2, "REF 41C CLICK: OK");
    assert_oled_row(3, "HOLD: LOCAL STOP");
    save_oled_frame("calibration-reference-error");
    for (unsigned i = 27; i < 41; ++i) {
        ++host_now; ui_key_callb(ec11_task_is_encode, -1); ui.run_task();
    }
    float captured_raw[4] = {adc.snapshot().raw_temp, 0, 0, 0};
    const uint16_t entered[] = {27, 105, 166, 207};
    press(sw_click); assert(pwm.calibration_status().point == 1);
    pwm_irq();
    assert(host_pwm[HEB_PIN] > 0);
    assert_oled_row(0, "P1 TARGET 100C");
    assert_oled_row(2, "HEATING TO TARGET");
    assert_raw_row();
    save_oled_frame("calibration-heating");

    // 室温及80℃的平坦平台都不能替代目标附近的新窗口。
    run_ui(33000, room_mv);
    press(sw_click); assert(pwm.calibration_status().point == 1);
    refresh_ui(); assert_oled_row(2, "HEATING TO TARGET");
    run_ui(40500, plateau_mv);
    press(sw_click); assert(pwm.calibration_status().point == 1);
    refresh_ui(); assert_oled_row(2, "HEATING TO TARGET");
    run_ui(1000, near_edge_mv);
    assert(std::fabs(adc.snapshot().instant_temp - calibration_target(1)) <= 2);
    assert(std::fabs(adc.snapshot().raw_temp - calibration_target(1)) > 2);
    refresh_ui(); assert_oled_row(2, "HEATING TO TARGET");
    for (unsigned i = 0; i < 8 &&
         !calibration_near_target(1, adc.snapshot().raw_temp, adc.snapshot().instant_temp); ++i)
        run_ui(1000, near_edge_mv);
    assert(calibration_near_target(1, adc.snapshot().raw_temp, adc.snapshot().instant_temp));
    run_ui(29000, near_edge_mv);
    press(sw_click); assert(pwm.calibration_status().point == 1);
    run_ui(10000, near_edge_mv);
    assert_oled_four_rows();
    assert_raw_row();
    set_reference(107);
    save_oled_frame("calibration-heated-input");

    // 瞬时值越出目标，即使均值及原窗口范围仍合格，也必须丢弃待录入值。
    const auto previous = adc.snapshot();
    host_adc_mv = outside_mv; host_now += 1000; adc.get_temp_task();
    const auto outside = adc.snapshot();
    assert(std::fabs(outside.raw_temp - calibration_target(1)) <= 2);
    assert(std::fabs(outside.instant_temp - calibration_target(1)) > 2);
    assert(std::fabs(outside.raw_temp - previous.raw_temp) < 1);
    assert(std::fabs(outside.instant_temp - previous.instant_temp) < 2);
    ui_key_callb(ec11_task_is_key, sw_click);
    ui.run_task(); pwm_irq();
    assert(pwm.calibration_status().point == 1);
    assert_oled_row(0, "P1 100C UNSTABLE");
    assert_oled_row(2, "HEATING TO TARGET");
    save_oled_frame("calibration-left-target");
    run_ui(29000, near_edge_mv);
    press(sw_click); assert(pwm.calibration_status().point == 1);
    run_ui(10000, near_edge_mv);
    char reset_value[22];
    std::snprintf(reset_value, sizeof(reset_value), "REF %uC CLICK: OK",
                  static_cast<unsigned>(std::lround(adc.snapshot().raw_temp)));
    assert_oled_row(2, reset_value);
    set_reference(entered[1]);
    captured_raw[1] = adc.snapshot().raw_temp;
    press(sw_click); assert(pwm.calibration_status().point == 2);
    run_ui(40500, mid_mv);
    assert_oled_row(0, "P2 TARGET 160C");
    assert_raw_row();
    set_reference(entered[2]);
    captured_raw[2] = adc.snapshot().raw_temp;
    press(sw_click); assert(pwm.calibration_status().point == 3);
    run_ui(40500, high_mv);
    assert_oled_row(0, "P3 TARGET 200C");
    assert_raw_row();
    set_reference(entered[3]);
    captured_raw[3] = adc.snapshot().raw_temp;
    const unsigned before = EEPROM.commits;
    press(sw_click);
    assert(!pwm.calibration_can_save() && EEPROM.commits == before);
    ui.run_task(); assert(EEPROM.commits == before);
    pwm_irq(); assert(pwm.calibration_can_save() && host_pwm[HEB_PIN] == 0);
    ui.run_task(); ui.run_task();
    assert(adc.has_calibration() && EEPROM.commits == before + 1);
    ui.run_task(); assert(EEPROM.commits == before + 1);
    TempCalibData persisted;
    assert(decode_temp_calib(EEPROM.flash.data() + temp_calib_record_add, persisted));
    for (unsigned point = 0; point < CALIB_POINT_NUM; ++point) {
        assert(std::fabs(persisted.raw_temp[point] - captured_raw[point]) <= 0.051f);
        assert(persisted.real_temp[point] == entered[point]);
        assert(persisted.real_temp[point] != calibration_target(point));
        close_to(adc.calibrate_temp(persisted.raw_temp[point]), entered[point]);
    }
    assert_oled_four_rows();
    save_oled_frame("calibration-success");
    press(sw_long);
    assert(page_num == 2 && ec11.speed_up_enabled() && ec11.double_click_enabled());
    adc.clear_calibration(); eeprom.read_all_data();
    assert(adc.has_calibration());
    for (unsigned point = 0; point < CALIB_POINT_NUM; ++point)
        close_to(adc.calibrate_temp(persisted.raw_temp[point]), entered[point]);
    room_and_unlock();
    enter_ui(); run_ui(32500, room_mv); press(sw_click); pwm_irq();
    assert(host_pwm[HEB_PIN] > 0);
    const unsigned unchanged = EEPROM.commits;
    host_isr = true;
    for (unsigned i = 0; i < 64; ++i) ui_key_callb(ec11_task_is_encode, 1);
    ui_key_callb(ec11_task_is_key, sw_long);
    host_isr = false;
    pwm_irq(); stopped(CalibStop::Cancelled);
    ui.run_task();
    assert(page_num == 2 && EEPROM.commits == unchanged && eeprom.write_flg == 0);
    std::cout << "PASS UI: near-target windows, REF target loss, external pairs/reload, visible errors, stop-before-save, queue-full cancel\n";
}

static void network_calibration_checks() {
    room_and_unlock();
    eeprom.write_flg = 0;
    miot.miot_able = true;
    miot.wifi_conect_flg = true;
    enter_ui();
    run_ui(32500, room_mv);
    press(sw_click);
    pwm_irq();
    assert(pwm.calibration_status().point == 1 && host_pwm[HEB_PIN] > 0);

    // 网络等待期间监督定时器仍执行，模拟主循环被同步调用阻塞。
    host_blinker_wait_ms = 1000;
    host_blinker_wait_tick = pwm_irq;
    const unsigned commits = EEPROM.commits;
    const uint32_t started = host_now;
    for (unsigned i = 0; i < 1000; ++i) {
        loop();
        pwm_irq();
        assert(pwm.calibration_active());
        assert(pwm.calibration_status().reason == CalibStop::None);
        assert(host_pwm[HEB_PIN] > 0);
    }
    assert(static_cast<uint32_t>(host_now - started) == 1000);
    assert(EEPROM.commits == commits);
    assert_oled_four_rows();
    assert_oled_row(0, "P1 TARGET 100C");
    assert_raw_row();
    assert_oled_row(2, "HEATING TO TARGET");
    assert_oled_row(3, "HOLD:STOP / NET PAUSE");
    save_oled_frame("calibration-network-paused");

    ui_key_callb(ec11_task_is_key, sw_long);
    loop();
    pwm_irq();
    stopped(CalibStop::Cancelled);
    assert(page_num == 2 && EEPROM.commits == commits);
    const uint32_t stopped_at = host_now;
    loop();
    assert(static_cast<uint32_t>(host_now - stopped_at) >= 1000);
    assert(host_pwm[HEB_PIN] == 0 && !pwm.power);

    host_blinker_wait_ms = 0;
    host_blinker_wait_tick = nullptr;
    miot.miot_able = false;
    miot.wifi_conect_flg = false;
    room_and_unlock();
    std::cout << "PASS network calibration: real loop survives blocking network, local stop, networking resumes without reheating\n";
}
int main() {
    model_checks();
    oled_checks();
    controller_checks();
    room_mv = voltage_for(25); warm_mv = voltage_for(100); mid_mv = voltage_for(160);
    high_mv = voltage_for(200); trip_mv = voltage_for(235);
    plateau_mv = voltage_for(80); near_edge_mv = voltage_for(101.5f);
    outside_mv = voltage_for(102.5f);
    adc_checks(); storage_checks(); stability_checks(); pwm_checks(); ui_checks();
    network_calibration_checks();
    std::cout << "All calibration behavior checks passed; electrical timing and heating remain unverified.\n";
}
