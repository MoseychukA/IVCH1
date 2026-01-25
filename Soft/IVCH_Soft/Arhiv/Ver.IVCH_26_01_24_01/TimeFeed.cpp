#include "TimeFeed.h"

char gDateStr[11] = "00.00.0000";
char gTimeStr[9] = "00:00:00";
volatile bool gTimeUpdated = false;