#include "User.h"
#include "bmp.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

namespace
{
struct KeyEvent
{
    ec11_task_result_type type;
    int16_t value;
    uint32_t at;
    uint32_t generation;
};
constexpr uint8_t key_queue_capacity = 32;
KeyEvent key_queue[key_queue_capacity];
portMUX_TYPE key_mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t key_head = 0;
uint8_t key_count = 0;
uint32_t key_generation = 0;
bool calib_page_armed = false;
bool calib_stop_armed = false;
bool calib_exit_requested = false;

void key_lock(bool isr)
{
    if (isr) {
        portENTER_CRITICAL_ISR(&key_mux);
    } else {
        portENTER_CRITICAL(&key_mux);
    }
}

void key_unlock(bool isr)
{
    if (isr) {
        portEXIT_CRITICAL_ISR(&key_mux);
    } else {
        portEXIT_CRITICAL(&key_mux);
    }
}

void discard_keys()
{
    key_lock(false);
    key_head = 0;
    key_count = 0;
    ++key_generation;
    key_unlock(false);
}

bool take_calib_exit()
{
    key_lock(false);
    const bool requested = calib_exit_requested;
    calib_exit_requested = false;
    key_unlock(false);
    return requested;
}

const char *calib_stop_text(CalibStop reason)
{
    switch (reason)
    {
    case CalibStop::Cancelled: return "CANCELLED";
    case CalibStop::RemoteOff: return "REMOTE OFF";
    case CalibStop::Sensor: return "SENSOR ERROR";
    case CalibStop::OverTemp: return "OVER TEMPERATURE";
    case CalibStop::SampleTimeout: return "SAMPLE TIMEOUT";
    case CalibStop::HeartbeatTimeout: return "UI TIMEOUT";
    case CalibStop::PointTimeout: return "POINT TIMEOUT";
    case CalibStop::SessionTimeout: return "SESSION TIMEOUT";
    default: return "STOPPED";
    }
}
}
UI ui;

#define page2_menu_num_max 8

typedef enum
{
    temp_mode_menu_num,
    backflow_menu_num,
    constant_temperature_menu_num,
    iot_menu_num,
    oled_light_menu_num,
    use_menu_num,
    about_menu_num,
    error_temp_fix_num
} page2_menu_num_type;

int8_t page_num = 0;

/*菜单页定义*/
int8_t page2_menu_num = 0;
int8_t page2_move_tmp = 0;
int8_t page2_move_flg = 0;

const char page2_menu0[] = {"温控模式"};
const char page2_menu1[] = {"回流参数"};
const char page2_menu2[] = {"恒温参数"};
const char page2_menu3[] = {"物联网"};
const char page2_menu4[] = {"屏幕亮度"};
const char page2_menu5[] = {"使用说明"};
const char page2_menu6[] = {"关于"};
const char page2_menu7[] = {"温度校准"};

const char *page2_str_ptr[] = {page2_menu0, page2_menu1, page2_menu2, page2_menu3, page2_menu4, page2_menu5, page2_menu6, page2_menu7};
const char *page2_bmp_ptr[] = {page2_bmp0, page2_bmp1, page2_bmp2, page2_bmp3, page2_bmp4, page2_bmp5, page2_bmp6, page2_bmp7};
/************************/

/*设置页定义*/
int8_t page3_switch_flg = 0;

const char menu0_option0[] = {"回流焊模式"};
const char menu0_option1[] = {"恒温模式"};

const char menu1_option0[] = {"活性区"};
const char menu1_option1[] = {"回流区"};
const char menu1_option_s[] = {"秒"};
const char menu1_option_t[] = {"摄"};
int8_t temp_mode0_option = 0; //回流曲线设置项 0：活性区温度 1：活性区时间 2：回流区温度 3：回流区时间
short curve_temp_buf[] = {    // 0：保温区温度110-200 1：保温区时长60-120 2：回流区温度220-上限  3：回流区时长 30-90
    140, 90, 255, 60};

const char menu2_option0[] = {"分钟"};

const char menu3_option0_0[] = {"单击开启"};
const char menu3_option0_1[] = {"单击关闭"};

const char menu3_option1_0[] = {"单击配置网络密匙"};
const char menu3_option1_1[] = {"请连接至热点"};
const char menu3_option1_2[] = {"QF_HP"};

const char menu5_option0_0[] = {"这么简单自己摸索"};

const char menu6_option0_0[] = {"启凡科创智能加热台"};
const char menu6_option0_1[] = {"Version:V1.5"};

// 校准页使用四行小字，避免三十二像素屏幕裁切关键提示。

/************************/

void UI::run_task()
{

    consume_key_events();
    calibration_tick(millis());
    if (calib_state != CalibState::IDLE)
    {
        oled_sleep_t = 0;
        wake_sleep_change_flg = 0;
        if (oled_sleep_flg)
        {
            oled.display_on();
            oled_sleep_flg = false;
        }
        const uint32_t now = millis();
        if (calib_dirty || static_cast<uint32_t>(now - calib_draw_at) >= 200)
        {
            calib_draw_at = now;
            calib_dirty = false;
            oled.clr();
            oled.xy_set(0, 0, 128, 4);
            calibration_draw(0, 0);
            oled.refresh();
        }
        return;
    }
    wake_sleep_page();

    if (oled_sleep_flg)
        return;

    switch (page_num)
    {
    case 1:
        temp_move();
        temp_mode_move();
        heat_move();
        temp_time_switch();
        if(show_warning_flg)
        {
            show_warning();
            show_warning_flg = 0;
        }
        break;
    case 2:
        page2_move();
        break;
    case 3:
        page3_switch();
        blinker_config();
        break;
    }
    
    if (page2_move_flg)
        return;

    page_switch(switch_buf);

    if (!oled_flg)
        return;
    oled_flg = 0;
    oled.clr();

    show_page(0, 0, page_num);
    write_oled_light();

    oled.refresh();
    //pwm.fan_PWM();
    pwm.fan_time();
}

void UI::show_warning()
{
    for (int8_t i = 32; i >= 0; i--)
    {
        oled.clr();
        show_page(0, i, 4);
        show_page(0, i - 32, 1);
        oled.refresh();
        delay(1);
    }
    delay(1000);
    for (int8_t i = 0; i < 32; i++)
    {
        oled.clr();
        show_page(0, i, 4);
        show_page(0, i - 32, 1);
        oled.refresh();
        delay(1);
    }
}

void UI::page1_key(ec11_task_result_type ec_type, int16_t ec_value) //主界面按键对应功能
{
    if (ec_type == ec11_task_is_key)
    {
        switch (ec_value)
        {
        case sw_click:
            if (pwm.power){
                pwm.end();
                pwm.fan();
                
            }else{
                pwm.begin();
                //pwm.fan();
                //analogWrite(FAN_IO, 64);
            }
            break;
        case sw_long:
            switch_buf = sure;
            page_switch_flg = 1;
            ec11.speed_up(false);
            break;
        case sw_double:
            pwm.fan();
            break;

        default:
            break;
        }
    }
    else
    {
        if (pwm.temp_mode)
        {
            pwm.temp_buf += ec_value*5;

            if (pwm.temp_buf > adc.hotbed_max_temp)
                pwm.temp_buf = adc.hotbed_max_temp;
            else if (pwm.temp_buf < 40)
                pwm.temp_buf = 40;
            eeprom.write_flg = 1;
            if (show_temp_mode != show_set_temp)
            {
                show_temp_mode = show_set_temp;
                temp_move_flg = 1;
            }
        }
        else
        {
            show_warning_flg = 1;
        }
    }
}

void UI::page2_key(ec11_task_result_type ec_type, int16_t ec_value) //界面2按键对应功能
{
    if (ec_type == ec11_task_is_key)
    {
        switch (ec_value)
        {

        case sw_long:
            if (page2_menu_num == error_temp_fix_num)
            {
                calibration_enter();
                return;
            }
            switch_buf = sure;
            page_switch_flg = 1;
            if (page2_menu_num == constant_temperature_menu_num || page2_menu_num == oled_light_menu_num || page2_menu_num == backflow_menu_num)
                ec11.speed_up(true);
            ec11.double_click(false);
            break;
        case sw_double:
            switch_buf = 0;
            page_switch_flg = 1;
            ec11.speed_up(true);
            break;

        default:
            break;
        }
    }
    else
    {
        if (page2_move_tmp < 0 && ec_value == 1)
            page2_move_tmp = -1;
        else if (page2_move_tmp > 0 && ec_value == -1)
            page2_move_tmp = 1;
        page2_move_tmp += ec_value;
        page2_move_flg = 1;
    }
}

void UI::page3_key(ec11_task_result_type ec_type, int16_t ec_value) //界面3按键对应功能
{
    if (page2_menu_num == error_temp_fix_num)
    {
        calibration_key(ec_type, ec_value);
        return;
    }
    if (ec_type == ec11_task_is_key)
    {
        if (ec_value == sw_click) //单击
        {
            if (page2_menu_num == backflow_menu_num)
            {
                if (!page3_switch_flg)
                {
                    page3_switch_flg = 1;
                    temp_mode0_option++;
                    if (temp_mode0_option == 4)
                        temp_mode0_option = 0;
                }
            }

            if (page2_menu_num == iot_menu_num)
            {
                if (!page3_switch_flg)
                {
                    if (miot_option_buf)
                    {
                        blinker_config_flg = 1;
                    }
                    else
                    {
                        page3_switch_flg = 1;
                        miot.miot_able = !miot.miot_able;
                    }
                }
            }
        }
        else
        {
            if (wifima.wifima_flg)
            {
                wifima.back_flg = 1;
            }
            else
            {

                switch_buf = 0;
                page_switch_flg = 1;
                ec11.speed_up(false);
                if (page2_menu_num < 6)
                    eeprom.write_flg = 1;
                ec11.double_click(true);

                if (pwm.power && page2_menu_num == temp_mode_menu_num)
                {
                    pwm.end();
                }
                if (page2_menu_num == iot_menu_num)
                {
                    if (miot.miot_able && !miot.open_flg)
                    {
                        eeprom.write(miot_able_add, 1);
                        //ESP.reset();
                    }
                }
            }
        }
    }
    else
    {

        switch (page2_menu_num)
        {
        case temp_mode_menu_num: //温控模式
            if (!page3_switch_flg)
                pwm.temp_mode = !pwm.temp_mode;
            break;

        case backflow_menu_num: //回流参数
            if (!page3_switch_flg)
            {
                switch (temp_mode0_option)
                {
                case 0:
                    curve_temp_buf[0] += ec_value;
                    if (curve_temp_buf[0] < 110)
                        curve_temp_buf[0] = 110;
                    else if (curve_temp_buf[0] > 200)
                        curve_temp_buf[0] = 200;
                    break;

                case 1:
                    curve_temp_buf[1] += ec_value;
                    if (curve_temp_buf[1] < 60)
                        curve_temp_buf[1] = 60;
                    else if (curve_temp_buf[1] > 120)
                        curve_temp_buf[1] = 120;
                    break;

                case 2:
                    curve_temp_buf[2] += ec_value;
                    if (curve_temp_buf[2] < 220)
                        curve_temp_buf[2] = 220;
                    else if (curve_temp_buf[2] > adc.hotbed_max_temp)
                        curve_temp_buf[2] = adc.hotbed_max_temp;
                    break;

                case 3:
                    curve_temp_buf[3] += ec_value;
                    if (curve_temp_buf[3] < 30)
                        curve_temp_buf[3] = 30;
                    else if (curve_temp_buf[3] > 90)
                        curve_temp_buf[3] = 90;
                    break;
                }
            }
            break;

        case constant_temperature_menu_num: //恒温参数
            pwm.temp_mode1_time += ec_value;
            if (pwm.temp_mode1_time < 0)
                pwm.temp_mode1_time = 0;
            else if (pwm.temp_mode1_time > 520)
                pwm.temp_mode1_time = 520;
            break;

        case iot_menu_num: // iot
            if (!page3_switch_flg)
                miot_option_buf = !miot_option_buf;
            break;

        case oled_light_menu_num: // 屏幕亮度
            oled_light += ec_value;
            if (oled_light < 0)
                oled_light = 0;
            else if (oled_light > 255)
                oled_light = 255;
            write_oled_flg = 1;
            break;
        default:
            break;
        }
        if (page2_menu_num == temp_mode_menu_num || page2_menu_num == iot_menu_num)
            page3_switch_flg = ec_value;
    }
}

void UI::blinker_config()
{
    int8_t y;
    if (!blinker_config_flg)
        return;
    blinker_config_flg = 0;

    y = -1;
    for (;;)
    {
        if (y == -33)
            break;
        oled.clr();
        show_page(0, y, 3);
        oled.chinese(16, y + 32, menu3_option1_1, 16, 1, 0);
        oled.str(44, y + 48, menu3_option1_2, 16, 1, 0);
        oled.refresh();
        y--;
        yield();
    }
    Serial.println("Start ap config");
    wifima.startConfigPortal("QF_HP");
    Serial.println("End ap config");

    y = 1;
    for (;;)
    {
        if (y == 32)
            break;
        oled.clr();
        show_page(0, y - 32, 3);
        oled.chinese(16, y, menu3_option1_1, 16, 1, 0);
        oled.str(44, y + 16, menu3_option1_2, 16, 1, 0);
        oled.refresh();
        y++;
        yield();
    }
}

bool UI::oled_display_set()
{
    if (ui.wake_sleep_change_flg)
        return 1;
    if (!ui.oled_sleep_flg)
    {
        oled_sleep_t = 0;
        return 0;
    }
    else
    {
        ui.wake_sleep_change_flg = 1;
        oled_sleep_t = 0;
        return 1;
    }
}

void ui_key_callb(ec11_task_result_type ec_type, int16_t ec_value) // 中断与定时任务只提交事件。
{
    ui.enqueue_key_event(ec_type, ec_value);
}

void UI::enqueue_key_event(ec11_task_result_type ec_type, int16_t ec_value)
{
    const bool isr = xPortInIsrContext();
    const uint32_t now = millis();
    key_lock(isr);
    const bool long_press = ec_type == ec11_task_is_key && ec_value == sw_long;
    const bool stop = long_press && calib_stop_armed;
    if (long_press && calib_page_armed)
        calib_exit_requested = true;
    if (key_count < key_queue_capacity)
    {
        key_queue[(key_head + key_count) % key_queue_capacity] =
            {ec_type, ec_value, now, key_generation};
        ++key_count;
    }
    key_unlock(isr);
    // 独立停止请求不依赖普通队列容量，也不触碰显示或输出引脚。
    if (stop)
        pwm.request_calibration_stop(CalibStop::Cancelled);
}

void UI::consume_key_events()
{
    if (take_calib_exit())
    {
        if (calib_state != CalibState::IDLE)
            calibration_exit();
        return;
    }
    for (uint8_t i = 0; i < key_queue_capacity; ++i)
    {
        key_lock(false);
        if (!key_count)
        {
            key_unlock(false);
            break;
        }
        const KeyEvent event = key_queue[key_head];
        key_head = (key_head + 1) % key_queue_capacity;
        --key_count;
        const uint32_t generation = key_generation;
        key_unlock(false);
        if (take_calib_exit())
        {
            if (calib_state != CalibState::IDLE)
                calibration_exit();
            break;
        }
        if (event.generation != generation)
            continue;
        if (calib_state == CalibState::ENTER_VALUE &&
            static_cast<int32_t>(event.at - calib_input_after) <= 0)
            continue;
        // 输入前也更新快照，失稳时立即废弃尚未处理的录入事件。
        calibration_tick(millis());
        key_lock(false);
        const bool current = event.generation == key_generation;
        key_unlock(false);
        if (current)
            process_key_event(event.type, event.value);
    }
}

void UI::process_key_event(ec11_task_result_type ec_type, int16_t ec_value)
{
    if (calib_state != CalibState::IDLE)
    {
        calibration_key(ec_type, ec_value);
        return;
    }
    if (oled_display_set())
        return;
    switch (page_num)
    {
    case 1: page1_key(ec_type, ec_value); break;
    case 2: page2_key(ec_type, ec_value); break;
    case 3: page3_key(ec_type, ec_value); break;
    }
    eeprom.write_t = 0;
}

void UI::calibration_enter()
{
    calib_previous_speed = ec11.speed_up_enabled();
    calib_previous_double = ec11.double_click_enabled();
    ec11.speed_up(false);
    ec11.double_click(false);
    calib_state = CalibState::CONFIRM;
    calib_candidate = {};
    calib_point = 0;
    calib_message = "";
    calib_save_banner = false;
    calib_dirty = true;
    page_num = 3;
    page_switch_flg = false;
    page3_switch_flg = 0;
    page2_move_flg = 0;
    page2_move_tmp = 0;
    key_lock(false);
    calib_page_armed = true;
    calib_stop_armed = false;
    calib_exit_requested = false;
    key_head = key_count = 0;
    ++key_generation;
    key_unlock(false);
}

void UI::calibration_exit()
{
    key_lock(false);
    const bool stop = calib_stop_armed;
    calib_page_armed = false;
    calib_stop_armed = false;
    calib_exit_requested = false;
    key_head = key_count = 0;
    ++key_generation;
    key_unlock(false);
    if (stop)
        pwm.request_calibration_stop(CalibStop::Cancelled);
    calib_candidate = {};
    calib_window.reset();
    calib_state = CalibState::IDLE;
    ec11.speed_up(calib_previous_speed);
    ec11.double_click(calib_previous_double);
    page_num = 2;
    page_switch_flg = false;
    oled_flg = 1;
    // 返回菜单不写普通参数，也不恢复先前的加热任务。
}

void UI::calibration_reset_window()
{
    calib_window.reset();
    calib_window_sequence = adc.snapshot().sequence;
    calib_input_after = millis();
    discard_keys();
    calib_real = 0;
    calib_dirty = true;
}

void UI::calibration_abort(const char *message)
{
    calib_candidate = {};
    calib_window.reset();
    calib_state = CalibState::ABORT;
    calib_message = message;
    calib_dirty = true;
    discard_keys();
}

void UI::calibration_key(ec11_task_result_type ec_type, int16_t ec_value)
{
    if (ec_type == ec11_task_is_key && ec_value == sw_long)
    {
        calibration_exit();
        return;
    }
    if (ec_type == ec11_task_is_encode)
    {
        if (calib_state == CalibState::ENTER_VALUE)
        {
            // 校准页按方向每次一摄氏度，不继承进入前残留的加速计数。
            const int value = static_cast<int>(calib_real) + (ec_value > 0 ? 1 : ec_value < 0 ? -1 : 0);
            calib_real = value < 0 ? 0 : value > 230 ? 230 : value;
            calib_message = "";
            calib_dirty = true;
        }
        return;
    }
    if (ec_type != ec11_task_is_key || ec_value != sw_click)
        return;
    if (calib_state == CalibState::ABORT)
    {
        calibration_exit();
        return;
    }
    if (calib_state == CalibState::CONFIRM)
    {
        const TempSample sample = adc.snapshot();
        if (pwm.power || pwm.calibration_locked())
            calib_message = "BUSY / COOLING";
        else if (!sample.ready || !sample.healthy || !isfinite(sample.raw_temp) ||
                 static_cast<uint32_t>(millis() - sample.sampled_at) > CalibStability::max_gap_ms)
            calib_message = "WAIT HEALTHY SENSOR";
        else if (sample.raw_temp < 10.0f || sample.raw_temp > 40.0f)
            calib_message = "COOL TO 10..40C";
        else
        {
            key_lock(false);
            calib_stop_armed = true;
            key_unlock(false);
            if (pwm.start_calibration())
            {
                calib_candidate = {};
                calib_point = 0;
                calib_state = CalibState::WAIT_STABLE;
                calib_message = "";
                calibration_reset_window();
            }
            else
            {
                key_lock(false);
                calib_stop_armed = false;
                key_unlock(false);
                calib_message = "START REFUSED";
            }
        }
        calib_dirty = true;
        return;
    }
    if (calib_state != CalibState::ENTER_VALUE)
        return;
    const uint32_t now = millis();
    calibration_tick(now);
    if (calib_state != CalibState::ENTER_VALUE || !calib_window.stable(now))
        return;
    const float raw = calib_window.mean();
    if (calib_point == 0 &&
        (raw < 10.0f || raw > 40.0f || calib_real < 10 || calib_real > 40 ||
         fabsf(raw - static_cast<float>(calib_real)) > 10.0f))
    {
        calib_message = "P0 ROOM / CHECK REF";
        calib_dirty = true;
        return;
    }
    TempCalibData candidate = calib_candidate;
    candidate.raw_temp[calib_point] = raw;
    candidate.real_temp[calib_point] = calib_real;
    if (!validate_temp_calib(candidate, calib_point + 1))
    {
        calib_message = "POINT GAP / SLOPE";
        calib_dirty = true;
        return;
    }
    calib_candidate = candidate;
    if (++calib_point == CALIB_POINT_NUM)
    {
        // 完成仅是停止请求；实际输出已归零之前绝不允许提交闪存。
        pwm.request_calibration_stop(CalibStop::Completed);
        calib_state = CalibState::SAVING;
        calib_save_banner = false;
        calib_dirty = true;
        discard_keys();
    }
    else if (pwm.set_calibration_point(calib_point))
    {
        calib_state = CalibState::WAIT_STABLE;
        calib_message = "";
        calibration_reset_window();
    }
    else
    {
        pwm.request_calibration_stop(CalibStop::Cancelled);
        calibration_abort("POINT REQUEST FAILED");
    }
}

void UI::calibration_tick(uint32_t now)
{
    if (calib_state == CalibState::IDLE)
        return;
    if (take_calib_exit())
    {
        calibration_exit();
        return;
    }
    pwm.calibration_heartbeat();
    const CalibStatus status = pwm.calibration_status();
    const bool running = calib_state == CalibState::WAIT_STABLE || calib_state == CalibState::ENTER_VALUE;
    if ((running || calib_state == CalibState::SAVING) &&
        status.reason != CalibStop::None && status.reason != CalibStop::Completed)
    {
        calibration_abort(calib_stop_text(status.reason));
        return;
    }
    if (running && !status.active)
    {
        calibration_abort("SESSION STOPPED");
        return;
    }
    if (running)
    {
        const TempSample sample = adc.snapshot();
        if (sample.sequence != calib_window_sequence)
            calib_window.observe(sample.raw_temp, sample.instant_temp, sample.sampled_at,
                                 sample.sequence, sample.healthy, sample.ready, now);
        const bool stable = sample.healthy && sample.ready &&
            static_cast<uint32_t>(now - sample.sampled_at) <= CalibStability::max_gap_ms &&
            calib_window.stable(now);
        if (calib_state == CalibState::ENTER_VALUE && !stable)
        {
            calib_state = CalibState::WAIT_STABLE;
            calib_message = "UNSTABLE: WAIT AGAIN";
            calibration_reset_window();
        }
        else if (calib_state == CalibState::WAIT_STABLE && stable)
        {
            const float raw = calib_window.mean();
            if (calib_point == 0 && (raw < 10.0f || raw > 40.0f))
            {
                calib_message = "COOL / CHECK SENSOR";
                return;
            }
            calib_state = CalibState::ENTER_VALUE;
            calib_real = static_cast<uint16_t>(raw < 0 ? 0 : raw > 230 ? 230 : raw + 0.5f);
            calib_message = "";
            calib_input_after = now;
            discard_keys();
            calib_dirty = true;
        }
    }
    else if (calib_state == CalibState::SAVING && pwm.calibration_can_save())
    {
        if (!validate_temp_calib(calib_candidate))
        {
            calibration_abort("MODEL INVALID");
            return;
        }
        if (!calib_save_banner)
        {
            calib_dirty = true;
            return;
        }
        if (take_calib_exit())
        {
            calibration_exit();
            return;
        }
        if (!pwm.calibration_can_save())
            return;
        // 同步保存已经开始后不可撤销；中途长按只退出显示，不声称回滚。
        const bool saved = eeprom.save_calib_data(calib_candidate);
        if (take_calib_exit())
        {
            calibration_exit();
            return;
        }
        key_lock(false);
        calib_stop_armed = false;
        key_unlock(false);
        if (saved)
        {
            calib_state = CalibState::SUCCESS;
            calib_success_at = millis();
            calib_dirty = true;
            discard_keys();
        }
        else
            calibration_abort("FLASH SAVE FAILED");
    }
    else if (calib_state == CalibState::SUCCESS &&
             static_cast<uint32_t>(now - calib_success_at) >= 2000)
        calibration_exit();
}

void UI::calibration_draw(short x, short y)
{
    char line[22];
    const char *row0 = "";
    const char *row1 = "";
    const char *row2 = "";
    const char *row3 = "HOLD: CANCEL";
    if (calib_state == CalibState::CONFIRM)
    {
        row0 = "START CALIB?";
        row1 = "COLD 10..40C / REF";
        row2 = *calib_message ? calib_message : "FIX SPOT / CLICK GO";
    }
    else if (calib_state == CalibState::WAIT_STABLE || calib_state == CalibState::ENTER_VALUE)
    {
        const TempSample sample = adc.snapshot();
        static const unsigned budget[] = {0, 15, 35, 60};
        snprintf(line, sizeof(line), "P%u %u%% RAW %.1fC", calib_point, budget[calib_point],
                 static_cast<double>(sample.raw_temp));
        oled.str(x, y, line, 8, 1, 0);
        if (calib_state == CalibState::ENTER_VALUE)
            snprintf(line, sizeof(line), "REF %uC CLICK: OK", calib_real);
        else
        {
            unsigned seconds = calib_window.span_ms() / 1000;
            if (seconds > 30) seconds = 30;
            snprintf(line, sizeof(line), "WAIT STABLE %u/30s", seconds);
        }
        oled.str(x, y + 8, line, 8, 1, 0);
        row2 = *calib_message ? calib_message :
            calib_point == 0 ? "ROOM REF / FIX SPOT" : "FIX SPOT / EXT REF";
        oled.str(x, y + 16, row2, 8, 1, 0);
        oled.str(x, y + 24, row3, 8, 1, 0);
        return;
    }
    else if (calib_state == CalibState::SAVING)
    {
        if (pwm.calibration_can_save())
            calib_save_banner = true;
        row0 = calib_save_banner ? "SAVING..." : "STOPPING HEAT...";
        row1 = "FLASH CANNOT UNDO";
        row2 = "HOLD: EXIT DISPLAY";
        row3 = "FAN: COOLING";
    }
    else if (calib_state == CalibState::SUCCESS)
    {
        row0 = "CALIB SUCCESS!";
        row1 = "SAVED + READ VERIFIED";
        row2 = "NO AUTO HEATING";
        row3 = "FAN: COOLING";
    }
    else if (calib_state == CalibState::ABORT)
    {
        row0 = "CALIB ABORTED";
        row1 = calib_message;
        row2 = "OLD MODEL KEPT";
        row3 = "CLICK/HOLD: BACK";
    }
    oled.str(x, y, row0, 8, 1, 0);
    oled.str(x, y + 8, row1, 8, 1, 0);
    oled.str(x, y + 16, row2, 8, 1, 0);
    oled.str(x, y + 24, row3, 8, 1, 0);
}

void UI::write_oled_light()
{
    if (write_oled_flg)
    {
        write_oled_flg = 0;
        oled.light(oled_light);
    }
}

bool UI::page_switch(uint8_t mode)
{
    if (!page_switch_flg)
        return 0;
    page_switch_flg = false;

    int8_t next_page;
    int8_t show_y = 0;
    int8_t next_y;
    if (mode == 0)
    {
        next_page = page_num - 1;
        next_y = -32;
    }
    else if (mode == sure)
    {
        next_page = page_num + 1;
        next_y = 32;
    }
    else
        return 0;

    for (;;)
    {
        oled.clr();
        show_page(0, show_y, page_num);
        show_page(0, next_y, next_page);
        oled.refresh();

        if (mode == 0)
        {
            show_y++;
            next_y++;
        }
        else
        {
            show_y--;
            next_y--;
        }
        if (show_y == 33 || show_y == -33)
            break;
        yield();
    }

    page_num = next_page;

    return 1;
}

void UI::show_page(short x, short y, uint8_t page)
{
    uint8_t mode_tmp = ui.show_temp_mode;
    switch (page)
    {
    case 1:
        if (show_temp_mode == show_now_temp)
            show_temp(x, y, 93, y + 18);
        else
            show_temp(x, y, 0, 0);
        oled.xy_set(0, 0, 128, 4);
        if (pwm.temp_mode == Re_So)
            oled.chinese(69, y, "回流", 16, 1, 0);
        else
            oled.chinese(69, y, "恒温", 16, 1, 0);
        if (pwm.power)
            oled.BMP(95, y + 2, 32, 28, heating, 1);
        break;
    case 2:

        oled.xy_set(0, 0, 128, 4);
        oled.chinese(64, y + 8, page2_str_ptr[page2_menu_num], 16, 1, 0);
        oled.BMP(y, page2_bmp_ptr[page2_menu_num]);
        break;
    case 3:
        switch (page2_menu_num)
        {
        case temp_mode_menu_num: //模式设置
            if (pwm.temp_mode)
                oled.chinese(32, y + 8, menu0_option1, 16, 1, 0);
            else
                oled.chinese(24, y + 8, menu0_option0, 16, 1, 0);
            break;

        case backflow_menu_num: //回流曲线
            if (page3_switch_flg)
            {
                show_curve(0, y);
            }
            else
            {
                show_curve(y, y);
            }
            break;

        case constant_temperature_menu_num: //恒温时长
            ui.show_temp_mode = show_temp_mode1_time;
            show_temp(12, y, 0, 0);
            oled.chinese(84, y + 16, menu2_option0, 16, 1, 0);
            ui.show_temp_mode = mode_tmp;
            break;

        case iot_menu_num: //物联网
            if (miot_option_buf)
            {
                oled.chinese(0, y + 8, menu3_option1_0, 16, 1, 0);
            }
            else
            {
                if (miot.miot_able)
                    oled.chinese(32, y + 8, menu3_option0_1, 16, 1, 0);
                else
                    oled.chinese(32, y + 8, menu3_option0_0, 16, 1, 0);
            }
            break;

        case oled_light_menu_num: //屏幕亮度
            ui.show_temp_mode = show_set_light;
            show_temp(28, y, 0, 0);
            ui.show_temp_mode = mode_tmp;
            break;

        case use_menu_num: //使用说明
            oled.chinese(0, y + 8, menu5_option0_0, 16, 1, 0);
            //           oled.str(64, y + 8, menu5_option0_1, 16, 1, 0);
            break;

        case about_menu_num: //关于
            oled.chinese(0, y, menu6_option0_0, 16, 1, 1);
            oled.str(32, y + 16, menu6_option0_1, 16, 1, 0);
            break;

        case error_temp_fix_num: // 温度校准
            calibration_draw(x, y);
            break;
        }
        break;
    case 4: //显示提示
        oled.chinese(x + 8, y, "回流模式请到菜", 16, 1, 0);
        oled.chinese(x + 16, y + 16, "单内设置参数", 16, 1, 0);
        break;
    default:
        break;
    }
}

void UI::wake_sleep_page()
{
    if (wake_sleep_change_flg)
    {

        oled.display_on();
        if (oled_sleep_flg)
        {
            for (int8_t i = 32; i > 0; i--)
            {
                oled.clr();
                show_page(0, i, page_num);
                oled.refresh();
                yield();
            }
        }
        else
        {
            oled.roll(0, 0, 128, 4, 1, UP, 32);
            oled.display_off();
        }
        oled_sleep_flg = !oled_sleep_flg;
        wake_sleep_change_flg = 0;
    }
}

void UI::page3_switch()
{

    if (!page3_switch_flg)
        return;
    int8_t y;

    if (page3_switch_flg < 0)
    {
        oled.roll(0, 0, 128, 4, 2, DOWN, 16);
        y = -32;
    }
    else
    {
        if (page2_menu_num == backflow_menu_num)
            oled.roll(72, 0, 48, 4, 1, UP, 32);
        else
            oled.roll(0, 0, 128, 4, 2, UP, 16);
        y = 32;
    }

    for (;;)
    {
        if (y == 0)
            break;
        oled.clr();
        show_page(0, y, 3);
        oled.refresh();
        if (y < 0)
            y++;
        else
            y--;
        yield();
    }

    page3_switch_flg = 0;
}

void UI::page2_move()
{

    if (!page2_move_flg)
        return;
    int8_t num_tmp;
    int8_t now_y = 0;
    int8_t next_y;
    int8_t bmp_x;

    if (page2_move_tmp < 0)
    {
        num_tmp = page2_menu_num - 1;
        next_y = -32;
    }
    else
    {
        num_tmp = page2_menu_num + 1;
        next_y = 32;
    }
    if (num_tmp < 0)
        num_tmp = page2_menu_num_max - 1;
    else if (num_tmp == page2_menu_num_max)
        num_tmp = 0;

    for (;;)
    {
        if (page2_move_tmp == 0)
        {
            if (next_y < 0)
                page2_move_tmp = -1;
            else
                page2_move_tmp = 1;
        }
        now_y -= page2_move_tmp;
        next_y -= page2_move_tmp;
        if (now_y < -32 || now_y > 32)
            break;
        oled.clr();
        oled.chinese(64, now_y + 8, page2_str_ptr[page2_menu_num], 16, 1, 0);
        oled.BMP(now_y, page2_bmp_ptr[page2_menu_num]);

        oled.chinese(64, next_y + 8, page2_str_ptr[num_tmp], 16, 1, 0);
        oled.BMP(next_y, page2_bmp_ptr[num_tmp]);

        oled.refresh();
        yield();
    }
    if (page2_move_tmp < 0)
    {
        page2_move_tmp++;
    }
    else
    {
        page2_move_tmp--;
    }
    if (page2_move_tmp == 0)
        page2_move_flg = 0;
    page2_menu_num = num_tmp;
}

void UI::heat_move()
{
    if (!heat_flg)
        return;
    heat_flg = 0;
    int8_t y;
    if (pwm.power)
        y = 32;
    else
        y = 2;
    oled.xy_set(0, 0, 128, 4);
    for (;;)
    {
        if (pwm.power)
            y--;
        else
            y++;
        oled.choose_clr(101, 0, 26, 2);
        oled.choose_clr(95, 2, 32, 2);
        oled.BMP(95, y, 32, 28, heating, 1);
        oled.choose_refresh(95, 0, 32, 4);
        if (y == 2 || y == 32)
            return;
        yield();
    }
}

void UI::temp_mode_move()
{
    if (!temp_mode_flg)
        return;
    temp_mode_flg = 0;
    int8_t y = 0;
    oled.xy_set(0, 0, 128, 2);
    for (y = 0; y >= -16; y--)
    {
        oled.choose_clr(69, 0, 32, 2);
        if (pwm.temp_mode == Re_So)
        {
            oled.chinese(69, y + 16, "回流", 16, 1, 0);
            oled.chinese(69, y, "恒温", 16, 1, 0);
        }
        else
        {
            oled.chinese(69, y, "回流", 16, 1, 0);
            oled.chinese(69, y + 16, "恒温", 16, 1, 0);
        }
        oled.choose_refresh(69, 0, 32, 2);
        delay(8);
    }
}


void UI::temp_move()
{
    if (!temp_move_flg)
        return;
    temp_move_flg = 0;
    int8_t temp_x;
    uint8_t temp_y;
    uint8_t small_x;
    if (show_temp_mode == show_now_temp)
    {
        temp_x = 0;
    }
    else
    {
        oled.roll(0, 0, 68, 4, 2, UP, 16);
        temp_x = 68;
    }
    for (;;)
    {
        if (show_temp_mode == show_now_temp)
        {
            temp_x += 4;
        }
        else if (show_temp_mode == show_set_temp)
        {
            temp_x -= 4;
        }

        temp_y = temp_x * 1000 / 2125;
        small_x = temp_x * 100 / 283 + 69;

        oled.choose_clr(0, 0, 68, 4);
        oled.choose_clr(68, 2, 24, 2);
        show_temp(temp_x, temp_y, small_x, 18);

        oled.choose_refresh(0, 0, 68, 4);
        oled.choose_refresh(68, 2, 24, 2);
        if (temp_x >= 68 || temp_x <= 0)
            break;
        yield();
    }
    if (show_temp_mode == show_now_temp)
    {
        for (temp_y = 32; temp_y > 0; temp_y -= 2)
        {
            oled.choose_clr(0, 0, 68, 4);
            show_temp(0, temp_y, 0, 18);
            oled.choose_refresh(0, 0, 68, 4);
            yield();
        }
    }
}

void UI::show_temp(int8_t x, int8_t y, int8_t xx, int8_t yy)
{
    uint8_t dat_buf[3];
    uint16_t tmp;
    if (show_temp_mode == show_now_temp)
    {
        tmp = adc.now_temp;
        if (adc.now_temp <= 38)
            oled.BMP(x + 2, y, less);
    }
    else if (show_temp_mode == show_set_temp)
    {
        tmp = pwm.temp_buf;
    }
    else if (show_temp_mode == show_set_light)
    {
        tmp = oled_light;
    }
    else if (show_temp_mode == show_temp_mode1_time)
    {
        tmp = pwm.temp_mode1_time;
    }
    else
        return;
    oled.xy_set(68, 0, 128, 4);
    if (pwm.temp_mode)
    {
        if (pwm.temp_reached_flg)
            oled.num(xx, yy, temp_time_buf, 3, 16, RIGHT, 1);
        else
            oled.num(xx, yy, pwm.temp_buf, 3, 16, RIGHT, 1);
    }
    else
        oled.num(xx, yy, pwm.percent, 3, 16, RIGHT, 1);

    dat_buf[0] = tmp / 100 % 10;
    dat_buf[1] = tmp / 10 % 10;
    dat_buf[2] = tmp % 10;
    if (show_temp_mode < 3)
        oled.xy_set(0, 0, 68, 4);
    else
        oled.xy_set(0, 0, 128, 4);
    if (dat_buf[0])
        oled.BMP(x, y, 20, 32, number[dat_buf[0]], 1);
    if (dat_buf[0] || dat_buf[1])
        oled.BMP(x + 24, y, 20, 32, number[dat_buf[1]], 1);
    oled.BMP(x + 48, y, 20, 32, number[dat_buf[2]], 1);
}

void UI::show_curve(int8_t y, int8_t data_y)
{
    int8_t y_tmp;
    int8_t i;
    int8_t y_buf;
    int8_t x_tmp;
    int8_t x_buf;
    int8_t tmp;
    int8_t last_y;

    y_tmp = -((curve_temp_buf[0] - 40) * 100000 / 671875) + 32;
    y_buf = y_tmp;

    oled.point(11, y_buf + 1 + y, 1);

    for (i = 10; i > 0; i--) //升温曲线1
    {
        oled.point(i, ++y_buf + y, 1);
    }
    x_tmp = curve_temp_buf[1] / 5;
    x_buf = curve_temp_buf[3] / 5;

    for (i = 0; i < x_tmp; i++) //保温曲线
    {
        oled.point(12 + i, y_tmp + y, 1);
    }
    x_tmp += 12;

    y_tmp--;

    oled.point(x_tmp++, y_tmp + y, 1);

    y_buf = -((curve_temp_buf[2] - 40) * 100000 / 671875) + 32;

    for (i = 10; i > 0; i--) //升温曲线2
    {
        oled.point(x_tmp++, y_tmp + y, 1);
        y_tmp--;
        if (y_tmp == y_buf)
            break;
        if (y_tmp < 6)
        {
            x_buf -= 2;
        }
    }

    oled.point(x_tmp++, y_tmp + 1 + y, 1);
    x_buf -= 2;

    for (i = 0; i < x_buf; i++)
    {
        oled.point(x_tmp++, y_tmp + y, 1);
    }
    y_tmp++;
    oled.point(x_tmp++, y_tmp + y, 1);

    for (i = 0; i < 10; i++)
    {
        oled.point(x_tmp++, y_tmp + y, 1);
        y_tmp++;
        if (x_tmp == 72)
            break;
    }

    if (temp_mode0_option < 2)
        oled.chinese(72, data_y, menu1_option0, 16, 1, 0);
    else
        oled.chinese(72, data_y, menu1_option1, 16, 1, 0);

    oled.num(100, data_y + 16, curve_temp_buf[temp_mode0_option], 3, 16, RIGHT, 1);

    if (temp_mode0_option == 0 || temp_mode0_option == 2)
        oled.chinese(100, data_y + 16, menu1_option_t, 16, 1, 0);
    else
        oled.chinese(100, data_y + 16, menu1_option_s, 16, 1, 0);
}

void UI::temp_time_switch()
{
    if (temp_time_switch_flg == false)
        return;
    if (pwm.temp_reached_flg == false)
        min_count = 0;
    Serial.println(pwm.temp_reached_flg);
    pwm.temp_reached_flg = !pwm.temp_reached_flg;
    oled.roll(69, 2, 24, 2, 1, UP, 16);

    for (int8_t i = 32; i > 18; i--)
    {
        oled.choose_clr(69, 2, 24, 2);
        if (pwm.temp_reached_flg)
            oled.num(93, i, temp_time_buf, 3, 16, RIGHT, 1);
        else
            oled.num(93, i, pwm.temp_buf, 3, 16, RIGHT, 1);
        oled.choose_refresh(69, 2, 24, 2);
        delay(10);
    }
    temp_time_switch_flg = false;
}
