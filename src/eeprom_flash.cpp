#include "eeprom_flash.h"
#include "User.h"

#include <cstring>

eeprom_flash eeprom;

void eeprom_flash::data_init()
{
    // EEPROM初始化数据

    pwm.temp_buf = 100;
    pwm.temp_mode = 0;
    miot.miot_able = 0;
    pwm.temp_mode1_time = 10;
    ui.oled_light = 127;
    adc.adc_max_temp = 255;
    adc.hotbed_max_temp = 255;
    adc.clear_calibration();

    write_flg = 2;

    for (int x = first_download_add; x < eeprom_size; x++)
        EEPROM.write(x, 0);
}
void eeprom_flash::read_all_data()
{

    ec11.int_close();
    adc.clear_calibration();
    if (!EEPROM.begin(eeprom_size))
    {
        EEPROM.end();
        ec11.int_work();
        return;
    }
    if (EEPROM.read(first_download_add))
    {
        data_init();
    }
    else
    {
        pwm.temp_buf = ((uint16_t)(EEPROM.read(set_temp_high_add) << 8)) | EEPROM.read(set_temp_low_add);
        pwm.temp_mode = EEPROM.read(set_temp_mode_add);
        miot.miot_able = EEPROM.read(miot_able_add);
        pwm.temp_mode1_time = ((uint16_t)(EEPROM.read(temp_mode1_time_high_add) << 8)) | EEPROM.read(temp_mode1_time_low_add);
        ui.oled_light = EEPROM.read(set_oled_light_add);
        curve_temp_buf[0] = (EEPROM.read(temp0_data_start_add) * 256) + EEPROM.read(temp0_data_start_add + 1);
        curve_temp_buf[1] = (EEPROM.read(temp0_data_start_add + 2) * 256) + EEPROM.read(temp0_data_start_add + 3);
        curve_temp_buf[2] = (EEPROM.read(temp0_data_start_add + 4) * 256) + EEPROM.read(temp0_data_start_add + 5);
        curve_temp_buf[3] = (EEPROM.read(temp0_data_start_add + 6) * 256) + EEPROM.read(temp0_data_start_add + 7);
        char key[13] = {};
        bool key_valid = true;
        for (uint8_t i = 0; i < 12; ++i)
        {
            const uint8_t value = EEPROM.read(blinker_id_add + i);
            key[i] = static_cast<char>(value);
            if (value < 33 || value > 126)
                key_valid = false;
        }
        if (EEPROM.read(blinker_id_add + 12) != 0)
            key_valid = false;
        if (key_valid)
            wifima.blinker_id = String(key);
        Serial.print("Blinker ID:");
        Serial.println(wifima.blinker_id);
        adc.hotbed_max_temp = ((uint16_t)EEPROM.read(hotbed_max_temp_high_add) << 8) | EEPROM.read(hotbed_max_temp_low_add);
        if (adc.hotbed_max_temp > 270 || adc.hotbed_max_temp < 240)
        {
            adc.hotbed_max_temp = 255;
            EEPROM.write(hotbed_max_temp_low_add, adc.hotbed_max_temp & 0xff);
            EEPROM.write(hotbed_max_temp_high_add, adc.hotbed_max_temp >> 8);
        }
        adc.adc_max_temp = ((uint16_t)EEPROM.read(adc_max_temp_high_add) << 8) | EEPROM.read(adc_max_temp_low_add);
        if (adc.adc_max_temp > 400 || adc.adc_max_temp < 190)
        {
            adc.adc_max_temp = 255;
            EEPROM.write(adc_max_temp_low_add, adc.adc_max_temp & 0xff);
            EEPROM.write(adc_max_temp_high_add, adc.adc_max_temp >> 8);
        }

        uint8_t record[CALIB_RECORD_SIZE];
        for (size_t i = 0; i < CALIB_RECORD_SIZE; ++i)
            record[i] = EEPROM.read(temp_calib_record_add + i);
        TempCalibData loaded;
        if (decode_temp_calib(record, loaded))
            adc.set_calibration(loaded);
    }
    EEPROM.commit();
    EEPROM.end();

    ec11.int_work();
    oled.light(ui.oled_light);
}
uint8_t eeprom_flash::read(int add)
{
    uint8_t x;
    EEPROM.begin(eeprom_size);
    x = EEPROM.read(add);
    EEPROM.end();
    return x;
}
void eeprom_flash::write(int add, uint8_t dat)
{

    EEPROM.begin(eeprom_size);
    EEPROM.write(add, dat);
    EEPROM.end();
}

void eeprom_flash::write_task()
{
    if (write_flg == 2 && !pwm.calibration_active())
    {
        ec11.int_close();
        Serial.println("EE WRITE");

        if (!EEPROM.begin(eeprom_size))
        {
            EEPROM.end();
            ec11.int_work();
            return;
        }

        EEPROM.write(set_temp_low_add, pwm.temp_buf & 0xff);
        EEPROM.write(set_temp_high_add, pwm.temp_buf >> 8);

        EEPROM.write(set_temp_mode_add, pwm.temp_mode);

        EEPROM.write(miot_able_add, miot.miot_able);

        EEPROM.write(temp_mode1_time_low_add, pwm.temp_mode1_time & 0xff);
        EEPROM.write(temp_mode1_time_high_add, pwm.temp_mode1_time >> 8);

        EEPROM.write(set_oled_light_add, ui.oled_light & 0xff);

        EEPROM.write(temp0_data_start_add, curve_temp_buf[0] >> 8);
        EEPROM.write(temp0_data_start_add + 1, curve_temp_buf[0] & 0xff);

        EEPROM.write(temp0_data_start_add + 2, curve_temp_buf[1] >> 8);
        EEPROM.write(temp0_data_start_add + 3, curve_temp_buf[1] & 0xff);

        EEPROM.write(temp0_data_start_add + 4, curve_temp_buf[2] >> 8);
        EEPROM.write(temp0_data_start_add + 5, curve_temp_buf[2] & 0xff);

        EEPROM.write(temp0_data_start_add + 6, curve_temp_buf[3] >> 8);
        EEPROM.write(temp0_data_start_add + 7, curve_temp_buf[3] & 0xff);

        EEPROM.write(hotbed_max_temp_low_add, adc.hotbed_max_temp & 0xff);
        EEPROM.write(hotbed_max_temp_high_add, adc.hotbed_max_temp >> 8);

        EEPROM.write(adc_max_temp_low_add, adc.adc_max_temp & 0xff);
        EEPROM.write(adc_max_temp_high_add, adc.adc_max_temp >> 8);

        const bool committed = EEPROM.commit();

        EEPROM.end();
        if (committed)
            write_flg = 0;

        Serial.println("END");
        ec11.int_work();
    }
}

bool eeprom_flash::save_calib_data(const TempCalibData &data)
{
    // 调用方仍须等待校准完成且输出归零；这里拒绝活动加热时提交或切换模型。
    if (pwm.power || pwm.calibration_active())
        return false;
    uint8_t expected[CALIB_RECORD_SIZE];
    if (!encode_temp_calib(data, expected))
        return false;

    if (!EEPROM.begin(eeprom_size))
    {
        EEPROM.end();
        return false;
    }
    for (size_t i = 0; i < CALIB_RECORD_SIZE; ++i)
        EEPROM.write(temp_calib_record_add + i, expected[i]);
    const bool committed = EEPROM.commit();
    EEPROM.end();
    if (!committed)
        return false;

    if (!EEPROM.begin(eeprom_size))
    {
        EEPROM.end();
        return false;
    }
    uint8_t actual[CALIB_RECORD_SIZE];
    for (size_t i = 0; i < CALIB_RECORD_SIZE; ++i)
        actual[i] = EEPROM.read(temp_calib_record_add + i);
    EEPROM.end();

    TempCalibData verified;
    if (std::memcmp(expected, actual, CALIB_RECORD_SIZE) != 0 ||
        !decode_temp_calib(actual, verified))
        return false;
    if (pwm.power || pwm.calibration_active())
        return false;
    // 使用读回的量化模型，确保本次运行和重启后的映射完全一致。
    return adc.set_calibration(verified);
}

void eeprom_flash::write_str(int add, String str)
{
    const size_t length = str.length();
    if (add < 0 || add >= eeprom_size || length >= static_cast<size_t>(eeprom_size - add))
        return;
    if (add == blinker_id_add)
    {
        if (length != 12)
            return;
        for (size_t i = 0; i < length; ++i)
            if (static_cast<uint8_t>(str[i]) < 33 || static_cast<uint8_t>(str[i]) > 126)
                return;
    }
    if (!EEPROM.begin(eeprom_size))
    {
        EEPROM.end();
        return;
    }
    for (size_t i = 0; i < length; ++i)
        EEPROM.write(add + i, static_cast<uint8_t>(str[i]));
    EEPROM.write(add + length, 0);
    EEPROM.end();
}
