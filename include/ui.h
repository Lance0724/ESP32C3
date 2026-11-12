#ifndef _ui_H_
#define _ui_H_

#include "User.h"
#include "EC11.h"
#include "CalibStability.h"
#include "TempCalibration.h"

#define show_disable 0
#define show_set_temp 1
#define show_now_temp 2
#define show_set_light 3
#define show_temp_mode1_time 4

//#define back 0
#define sure 1

void ui_key_callb(ec11_task_result_type ec_type, int16_t ec_value);
extern short curve_temp_buf[4];

class UI
{

public:
    void run_task();
    void enqueue_key_event(ec11_task_result_type ec_type, int16_t ec_value);
    void process_key_event(ec11_task_result_type ec_type, int16_t ec_value);

    bool page_switch(uint8_t mode);

    void page1_key(ec11_task_result_type ec_type, int16_t ec_value);
    void page2_key(ec11_task_result_type ec_type, int16_t ec_value);
    void page3_key(ec11_task_result_type ec_type, int16_t ec_value);

    bool oled_display_set();

    uint8_t show_temp_mode = show_now_temp;
    uint8_t temp_move_flg = 0; //温度显示切换标志
    uint8_t temp_mode_flg = 0; //加热模式切换标志
    uint8_t heat_flg = 0;      //加热开关切换标志
    bool page_switch_flg = 1;  //界面切换标志
    int8_t switch_buf = sure;
    bool temp_time_switch_flg = 0;
    bool oled_sleep_flg = 0;

    int16_t oled_light = 0;
    bool wake_sleep_change_flg = 0;

private:
    void show_temp(int8_t x, int8_t y, int8_t xx, int8_t yy);
    void show_page(short x, short y, uint8_t page);
    void temp_move();
    void temp_mode_move();
    void heat_move();
    void page2_move();
    void page3_switch();
    void blinker_config();
    void write_oled_light();
    void show_curve(int8_t y, int8_t data_y);
    void temp_time_switch();
    void wake_sleep_page();
    void consume_key_events();
    void calibration_enter();
    void calibration_exit();
    void calibration_key(ec11_task_result_type ec_type, int16_t ec_value);
    void calibration_tick(uint32_t now);
    void calibration_draw(short x, short y);
    void calibration_abort(const char *message);
    void calibration_reset_window();
    void show_warning();

    uint8_t set_temp_x = 92;
    //uint8 show_warning_flg = 0;
    uint8_t show_warning_flg = 0;

    int8_t miot_option_buf = 0;

    bool blinker_config_flg = 0;
    bool write_oled_flg = 0;
    enum class CalibState : uint8_t { IDLE, CONFIRM, WAIT_STABLE, ENTER_VALUE, SAVING, SUCCESS, ABORT };
    CalibState calib_state = CalibState::IDLE;
    CalibStability calib_window;
    TempCalibData calib_candidate;
    uint8_t calib_point = 0;
    uint16_t calib_real = 0;
    uint32_t calib_success_at = 0;
    uint32_t calib_draw_at = 0;
    uint32_t calib_input_after = 0;
    uint32_t calib_window_sequence = 0;
    const char *calib_message = "";
    bool calib_previous_speed = false;
    bool calib_previous_double = true;
    bool calib_save_banner = false;
    bool calib_dirty = true;
};

extern UI ui;
extern uint8_t show_temp_mode;

#endif
