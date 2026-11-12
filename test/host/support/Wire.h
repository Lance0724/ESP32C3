#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// 仅替代总线硬件，文字栅格化和屏幕缓冲区使用生产驱动。
struct HostWire {
    std::array<uint8_t, 129> transmission{};
    size_t size = 0;
    bool begin(int, int) { return true; }
    void setClock(uint32_t) {}
    void beginTransmission(uint8_t) { size = 0; }
    size_t write(uint8_t value) {
        if (size == transmission.size()) return 0;
        transmission[size++] = value;
        return 1;
    }
    uint8_t endTransmission(bool = true) { return 0; }
};
inline HostWire Wire;
