#pragma once
#include <Arduino.h>
#include <IPAddress.h>
#include <string.h>

// IPAddress -> "нулевой IP"
static inline bool isZeroIP(const IPAddress& ip) {
	return (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0);
}

// строка IP -> "нулевой/пустой"
static inline bool isZeroIpStr(const char* s) {
	return (s == nullptr) || (s[0] == 0) || (strcmp(s, "0.0.0.0") == 0);
}