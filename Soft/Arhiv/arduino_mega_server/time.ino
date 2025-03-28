/*
  Module Time
  part of Arduino Mega Server project
*/

void timeInit() {
  initStart("Time");
  if (timeProvider == TIME_NETWORK) {
    #ifdef NTP_FEATURE
      Serial.println(F(" NTP sync..."));
      setSyncProvider(getNtpTime);
    #endif
  }
  else if (timeProvider == TIME_RTC) {
    Serial.println(F(" RTC sync..."));
    setSyncProvider(RTC.get);
  } 
  else {
    Serial.print(F(" Time service stopped"));
  }
  initDone();
}

// Print Time

void printDigits(byte digits) {
  if (digits < 10) {Serial.print('0');}
  Serial.print(digits);
}

void printTime() {
  Serial.print(year()); Serial.print("-"); printDigits(month()); Serial.print("-"); printDigits(day());
  Serial.print(" ");
  printDigits(hour()); Serial.print(":"); printDigits(minute()); Serial.print(":"); printDigits(second());
}

// Time Stamp

//#define STAMP_MILLIS
unsigned long stampMarker = 0;

String spacesDelay(unsigned long delay, byte size) {
  String d = String(delay);
  String s = "";
  for (byte i = 0; i < size - d.length(); i++) {
     s += F(" ");
  }
  return s;
}

void timeStamp() { 
  printTime();
  unsigned long delay = millis() - stampMarker;

  #if defined(STAMP_MILLIS)
    Serial.print(spacesDelay(delay, 5) + F(" ["));
    Serial.print(delay);
    Serial.print(F("] "));
    stampMarker = millis();
  #else
    Serial.print(F(" "));
  #endif 
}

// Duration

void showDuration(time_t duration) {
  timeStamp(); Serial.print(F("(duration "));
  if (duration >= SECS_PER_DAY) {Serial.print(duration / SECS_PER_DAY);  Serial.print(F(" day "));  duration = duration % SECS_PER_DAY;}
  if (duration >= SECS_PER_HOUR){Serial.print(duration / SECS_PER_HOUR); Serial.print(F(" hour ")); duration = duration % SECS_PER_HOUR;}
  if (duration >= SECS_PER_MIN) {Serial.print(duration / SECS_PER_MIN);  Serial.print(F(" min "));  duration = duration % SECS_PER_MIN;}
  Serial.print(duration); Serial.print(F(" sec)"));
}

void checkEvent(time_t* prevEvent) {
  time_t duration = 0;
  time_t timeNow = now();
  
  if (*prevEvent > 0) {
    duration = timeNow - *prevEvent;
  }     
  if (duration > 0) {
    showDuration(duration);
  }  
  *prevEvent = timeNow;
}

// Uptime

String stringUptime() {
  char uptime[14];
  char zero[] = "0";
  char nop[]  = "";
  char* zh;
  char* zm;
  char* zs;

  unsigned long totalSeconds = millis() / 1000;
  unsigned long days = totalSeconds / 86400;
  unsigned long tsHours = totalSeconds - days * 86400;
  unsigned long hours = tsHours / 3600;
  unsigned long tsMinutes = tsHours - hours * 3600;
  unsigned long minutes = tsMinutes / 60;
  unsigned long seconds = tsMinutes - minutes * 60;

  if (hours   < 10) {zh = zero;} else {zh = nop;}
  if (minutes < 10) {zm = zero;} else {zm = nop;}
  if (seconds < 10) {zs = zero;} else {zs = nop;}

  sprintf(uptime, "%d %s%d:%s%d:%s%d", (int)days, zh, (int)hours, zm, (int)minutes, zs, (int)seconds);
  return String(uptime);
} // uptimeString()

// Lifer

byte lifer = 0;

void setLifer() {
  lifer++;
  if (lifer > 6) {lifer = 0;}
}

byte getLifer() {
  return lifer;
}

// NTP

#ifdef NTP_FEATURE

  void rtcSync() {
    setSyncProvider(getNtpTime);
    Serial.println(F("...getNtpTime... "));
    if (timeStatus() != timeNotSet) {
      Serial.println(F("...set!... "));
      time_t t = getNtpTime();
      RTC.set(t);
      setSyncProvider(RTC.get);
    }
  }


  void setNtpInterval() {
    if (timeStatus() == timeNotSet) {
      setSyncInterval(18);
    } else {
        //setSyncInterval(3600); // not work > 19 sec. (cycle20s)
      }
  }
#endif

// Time Works

void timeWorks() {
  #ifdef NTP_FEATURE
    if (cycle20s) {setNtpInterval();}
  #endif
  if (cycle1s) {setLifer();}
}

