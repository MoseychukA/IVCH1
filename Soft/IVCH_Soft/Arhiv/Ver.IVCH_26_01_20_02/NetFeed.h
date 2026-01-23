#pragma once
#include <Arduino.h>

// Данные для отображения на дисплее (обновляйте из loop() по данным SIM800TimeAsync)
extern volatile bool gNetUpdated;

// Сеть
extern bool gNetRegistered; // true если CREG=1 или 5
extern uint8_t gCregStat; // 0..5 (как в +CREG)

// Сигнал
extern int16_t gRssiDbm; // 0 если unknown
extern uint8_t gSignalBars; // 0..5
extern int8_t gCsqRssi; // 0..31,99=unknown
extern uint8_t gCsqBer; // 0..7,99=unknown

// Утилита:обновить кэш из SIM800TimeAsync (возвращает true если что-то изменилось)
class SIM800TimeAsync;
bool NetFeed_UpdateFromSim(const SIM800TimeAsync& sim);