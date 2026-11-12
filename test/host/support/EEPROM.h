#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

// 仅替代存储介质；被验证的读回、发布及错误处理使用真实固件代码。
class EEPROMClass {
public:
    std::array<uint8_t, 4096> flash{}, buffer{};
    bool fail_begin = false, fail_commit = false, corrupt_after_commit = false;
    bool opened = false, dirty = false;
    unsigned commits = 0, writes = 0;
    bool begin(size_t size) {
        if (fail_begin || size != flash.size()) return false;
        buffer = flash;
        opened = true;
        dirty = false;
        return true;
    }
    uint8_t read(int address) const {
        assert(opened && address >= 0 && address < 4096);
        return buffer[static_cast<size_t>(address)];
    }
    void write(int address, uint8_t value) {
        assert(opened && address >= 0 && address < 4096);
        ++writes;
        if (buffer[static_cast<size_t>(address)] != value) dirty = true;
        buffer[static_cast<size_t>(address)] = value;
    }
    bool commit() {
        if (!opened) return false;
        if (!dirty) return true;
        if (fail_commit) return false;
        flash = buffer;
        dirty = false;
        ++commits;
        if (corrupt_after_commit) flash[2484] ^= 1;
        return true;
    }
    void end() { if (opened) commit(); opened = false; }
};
inline EEPROMClass EEPROM;
