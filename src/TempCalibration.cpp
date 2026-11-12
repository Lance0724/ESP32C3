#include "TempCalibration.h"

#include <cmath>
#include <cstring>

namespace
{
constexpr uint8_t RECORD_MAGIC = 0xA5;
constexpr uint8_t RECORD_VERSION = 1;
constexpr size_t CRC_OFFSET = 20;

uint16_t read_u16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(bytes[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);
}

void write_u16(uint8_t *bytes, uint16_t value)
{
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t record_crc(const uint8_t *record)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < CRC_OFFSET; ++i)
    {
        crc ^= static_cast<uint16_t>(record[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}
}

bool validate_temp_calib(const TempCalibData &data, uint8_t count)
{
    if (count == 0 || count > CALIB_POINT_NUM)
        return false;

    for (uint8_t i = 0; i < count; ++i)
    {
        const float raw = data.raw_temp[i];
        const float real = static_cast<float>(data.real_temp[i]);
        if (!std::isfinite(raw) || raw < 0.0f || raw > 230.0f || real > 230.0f)
            return false;
        if (i == 0)
            continue;

        const float raw_delta = raw - data.raw_temp[i - 1];
        const float real_delta = real - static_cast<float>(data.real_temp[i - 1]);
        // 0.1℃量化值的浮点减法也会略低于10℃，容差远小于一个存储量化单位。
        constexpr float tolerance = 0.0001f;
        if (raw_delta < 10.0f - tolerance || real_delta < 5.0f)
            return false;
        if (real_delta < 0.25f * raw_delta - tolerance ||
            real_delta > 4.0f * raw_delta + tolerance)
            return false;
    }
    return true;
}

float map_calibrated_temp(const TempCalibData &data, float raw_t)
{
    uint8_t segment = 0;
    while (segment < CALIB_POINT_NUM - 2 && raw_t > data.raw_temp[segment + 1])
        ++segment;

    const float raw_delta = data.raw_temp[segment + 1] - data.raw_temp[segment];
    const float real_delta = static_cast<float>(data.real_temp[segment + 1]) -
                             static_cast<float>(data.real_temp[segment]);
    return static_cast<float>(data.real_temp[segment]) +
           real_delta / raw_delta * (raw_t - data.raw_temp[segment]);
}

bool encode_temp_calib(const TempCalibData &data, uint8_t *record)
{
    if (record == nullptr || !validate_temp_calib(data))
        return false;

    TempCalibData quantized = data;
    uint8_t encoded[CALIB_RECORD_SIZE] = {};
    encoded[0] = RECORD_MAGIC;
    encoded[1] = RECORD_VERSION;
    encoded[2] = CALIB_POINT_NUM;
    for (uint8_t i = 0; i < CALIB_POINT_NUM; ++i)
    {
        const uint16_t raw = static_cast<uint16_t>(std::lround(data.raw_temp[i] * 10.0f));
        quantized.raw_temp[i] = static_cast<float>(raw) / 10.0f;
        write_u16(encoded + 4 + i * 2, raw);
        write_u16(encoded + 12 + i * 2, data.real_temp[i]);
    }
    // 量化也可能改变间距或斜率，不能仅验证原浮点候选。
    if (!validate_temp_calib(quantized))
        return false;
    write_u16(encoded + CRC_OFFSET, record_crc(encoded));
    std::memcpy(record, encoded, CALIB_RECORD_SIZE);
    return true;
}

bool decode_temp_calib(const uint8_t *record, TempCalibData &data)
{
    if (record == nullptr || record[0] != RECORD_MAGIC || record[1] != RECORD_VERSION ||
        record[2] != CALIB_POINT_NUM || record[3] != 0 ||
        read_u16(record + CRC_OFFSET) != record_crc(record))
        return false;

    TempCalibData decoded;
    for (uint8_t i = 0; i < CALIB_POINT_NUM; ++i)
    {
        decoded.raw_temp[i] = static_cast<float>(read_u16(record + 4 + i * 2)) / 10.0f;
        decoded.real_temp[i] = read_u16(record + 12 + i * 2);
    }
    if (!validate_temp_calib(decoded))
        return false;
    data = decoded;
    return true;
}
