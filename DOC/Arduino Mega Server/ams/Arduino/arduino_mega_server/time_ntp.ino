/*
  Module Time / NTP
  part of Arduino Mega Server project
*/

#ifdef NTP_FEATURE

//byte TIME_IP[] = {129, 6, 15, 30}; // time-c.nist.gov
byte TIME_IP[] = {88, 147, 254, 235}; 
unsigned int TIME_PORT = 123;
#define timeZone 3

IPAddress timeServer(TIME_IP);
EthernetUDP Udp;

#define NTP_PACKET_SIZE 48
byte packetBuffer[NTP_PACKET_SIZE];


void ntpInit() {
  initStart("NTP");
  Udp.begin(TIME_PORT);
  Serial.print(" Local port: "); Serial.println(TIME_PORT);
  moduleNtp = MODULE_ENABLE;
  initDone();
}

time_t getNtpTime() {
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print(F(" Transmit NTP request ("));
  printIp(TIME_IP);
  Serial.print(F(":"));
  Serial.print(TIME_PORT);
  Serial.print(F(")"));
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F(" Receive NTP response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      setSyncInterval(3600);
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(F("No NTP response"));
  setSyncInterval(18);
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

#endif // NTP_FEATURE
