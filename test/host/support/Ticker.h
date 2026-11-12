#pragma once
#include <cstdint>
class Ticker {
public:
    void attach_ms(uint32_t, void (*callback)()) { callback_ = callback; }
    void attach(float, void (*callback)()) { callback_ = callback; }
    void detach() { callback_ = nullptr; }
    bool active() const { return callback_ != nullptr; }
private:
    void (*callback_)() = nullptr;
};
