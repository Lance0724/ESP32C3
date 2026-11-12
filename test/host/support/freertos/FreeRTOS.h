#pragma once
#include <cstdint>
using TickType_t = uint32_t;
constexpr int pdTRUE = 1;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
struct portMUX_TYPE {};
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
#define portENTER_CRITICAL_ISR(mux) ((void)(mux))
#define portEXIT_CRITICAL_ISR(mux) ((void)(mux))
