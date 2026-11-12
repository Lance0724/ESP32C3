#ifndef TEMP_CALIBRATION_H
#define TEMP_CALIBRATION_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t CALIB_POINT_NUM = 4;
constexpr size_t CALIB_RECORD_SIZE = 22;

struct TempCalibData
{
    float raw_temp[CALIB_POINT_NUM] = {};
    uint16_t real_temp[CALIB_POINT_NUM] = {};
};

bool validate_temp_calib(const TempCalibData &data, uint8_t count = CALIB_POINT_NUM);
float map_calibrated_temp(const TempCalibData &data, float raw_t);
bool encode_temp_calib(const TempCalibData &data, uint8_t *record);
bool decode_temp_calib(const uint8_t *record, TempCalibData &data);

#endif
