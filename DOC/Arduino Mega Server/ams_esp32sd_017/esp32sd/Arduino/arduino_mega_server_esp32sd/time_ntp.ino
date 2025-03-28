/*
  Module Time NTP
  part of Arduino Mega Server project
*/

#ifdef FEATURE_NTP

#define TIMEZONE 3
#define NTP_PORT 123
WiFiUDP udp;

byte TIME_IP[] = {129, 6, 15, 30}; // time-c.nist.gov
IPAddress timeServerIp(TIME_IP);

#define NTP_PACKET_SIZE 48
byte packetBuffer[NTP_PACKET_SIZE];

unsigned long ntpTime = 0;

unsigned long ntpTryCount = 0;

void initNtp() {
  initStart("NTP", false);
  udp.begin(NTP_PORT);
  Serial.print(F(" Server: ")); Serial.println(stringIp(TIME_IP));
  Serial.print(F(" Port:   ")); Serial.println(NTP_PORT);
  moduleNtp = ENABLE;
  initDone(false);
}

time_t getNtpTime() {
  getNTP();
  return ntpTime;
}

bool getNTP() {
  redOn();
  sendNTPpacket(timeServerIp);
  delay(1200); // 1000
  int cb = udp.parsePacket();
  if (!cb) {
    ntpTryCount++;
    Serial.print(F("not received (")); Serial.print(ntpTryCount); Serial.println(F(")"));
    redOff();
    return false;
  } else {
      ntpTryCount = 0;
      Serial.println(F("OK"));
      //Serial.print(F(" Packet length: ")); Serial.println(cb);
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
      unsigned long  lowWord = word(packetBuffer[42], packetBuffer[43]);
      unsigned long secsSince1900 = highWord << 16 | lowWord;
      const unsigned long seventyYears = 2208988800UL;
      unsigned long epoch = secsSince1900 - seventyYears;
      ntpTime = epoch + TIMEZONE * 3600;
      //Serial.print(F(" Unix time = ")); Serial.println(ntpTime);
    }
  redOff();
  return true;
} // getNTP()
 
unsigned long sendNTPpacket(IPAddress& address) {
  Serial.print(F(" NTP request: "));
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // stratum, or type of clock
  packetBuffer[2] = 6;          // polling Interval
  packetBuffer[3] = 0xEC;       // peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

#endif // FEATURE_NTP
