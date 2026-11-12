#pragma once
#include "FreeRTOS.h"
struct StaticSemaphore_t { bool taken = false; };
using SemaphoreHandle_t = StaticSemaphore_t*;
inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) { return storage; }
inline int xSemaphoreTake(SemaphoreHandle_t mutex, TickType_t) {
    if (mutex->taken) return 0;
    mutex->taken = true;
    return pdTRUE;
}
inline void xSemaphoreGive(SemaphoreHandle_t mutex) { mutex->taken = false; }
