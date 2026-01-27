#pragma once
#include <Arduino.h>

// Эти строки обновляются при получении времени от SIM800/RTC.
// TFT/Menu часть может читать их и рисовать.
extern char gDateStr[11]; // "dd.mm.yyyy"
extern char gTimeStr[9]; // "hh:mm:ss"
extern volatile bool gTimeUpdated; // флаг обновления