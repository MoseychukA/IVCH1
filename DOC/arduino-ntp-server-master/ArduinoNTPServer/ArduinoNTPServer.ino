#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include "DateTime.h"
#include "GPS.h"

#define DEBUG false

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 1, 177);

unsigned int NTP_PORT = 123;

const int NTP_PACKET_SIZE = 48;

byte packetBuffer[ NTP_PACKET_SIZE ];

EthernetUDP Udp;

// time of last set or update, so its after gps update
DateTime referenceTime;
DateTime originTime;
DateTime receiveTime;
DateTime transmitTime;

// GPS
static const int RXPin = 8, TXPin = 9;
static const uint32_t GPSBaud = 9600;
GPS gps(RXPin, TXPin, DEBUG);

void setup() {
  // pinMode(RXPin, INPUT);
  Serial.begin(115200);
  gps.begin(GPSBaud);

  while (!Serial);
  Ethernet.begin(mac, ip);
  Udp.begin(NTP_PORT);

  Serial.println("NTP Server is running.");
  referenceTime = gps.getZDA();
}

void loop() {
  // NTP
  // TODO: put in separate classes: `NTPServer` and `NTPPacket`
  // maybe not :)
  IPAddress remoteIP;
  int remotePort;
  int packetSize = Udp.parsePacket();

  if (packetSize) {
    Serial.println("Get UDP packet.");
    receiveTime = gps.getZDA();

    remoteIP = Udp.remoteIP();
    remotePort = Udp.remotePort();
    // Serial.print("IP: ");
    // Serial.println(remoteIP);
    // Serial.print("port: ");
    // Serial.println(remotePort);
    // We've received a packet, read the data from it
    // read the packet into the buffer
    Udp.read(packetBuffer, NTP_PACKET_SIZE);

    // the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, extract the two words:
    unsigned long highWordSecond = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWordSecond = word(packetBuffer[42], packetBuffer[43]);
    unsigned long highWordCentisecond = word(packetBuffer[44], packetBuffer[45]);
    unsigned long lowWordCentisecond = word(packetBuffer[46], packetBuffer[47]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    originTime.time(highWordSecond << 16 | lowWordSecond);
    originTime.centisecond(highWordCentisecond << 16 | lowWordCentisecond);

    sendNTPpacket(remoteIP, remotePort);
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress remoteIP, int remotePort) {
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  // LI: 0, Version: 3, Mode: 4 (server)
  packetBuffer[0] = 0b00011100;
  // Stratum, or type of clock
  packetBuffer[1] = 0b00000001;
  // Polling Interval
  packetBuffer[2] = 10;
  // Peer Clock Precision
  // log2(sec)
  // 0xFA <--> -6 <--> 0.01 s
  packetBuffer[3] = 0xFA;
  // 8 bytes of zero for Root Delay & Root Dispersion
  // G
  packetBuffer[12] = 71;
  // P
  packetBuffer[13] = 80;
  // S
  packetBuffer[14] = 83;
  packetBuffer[15] = 0;

  // Reference Time
  packetBuffer[16] = (referenceTime.ntptime() & 0xFF000000) >> 24;
  packetBuffer[17] = (referenceTime.ntptime() & 0x00FF0000) >> 16;
  packetBuffer[18] = (referenceTime.ntptime() & 0x0000FF00) >> 8;
  packetBuffer[19] = (referenceTime.ntptime() & 0x000000FF);
  byte refCent[4];
  d2ba(1.0 * referenceTime.centisecond() / 100, refCent);
  packetBuffer[20] = refCent[0];
  packetBuffer[21] = refCent[1];
  packetBuffer[22] = refCent[2];
  packetBuffer[23] = refCent[3];

  // Origin Time
  packetBuffer[24] = (originTime.ntptime() & 0xFF000000) >> 24;
  packetBuffer[25] = (originTime.ntptime() & 0x00FF0000) >> 16;
  packetBuffer[26] = (originTime.ntptime() & 0x0000FF00) >> 8;
  packetBuffer[27] = (originTime.ntptime() & 0x000000FF);
  packetBuffer[28] = (originTime.centisecond() & 0xFF000000) >> 24;
  packetBuffer[29] = (originTime.centisecond() & 0x00FF0000) >> 16;
  packetBuffer[30] = (originTime.centisecond() & 0x0000FF00) >> 8;
  packetBuffer[31] = (originTime.centisecond() & 0x000000FF);

  // Receive Time
  packetBuffer[32] = (receiveTime.ntptime() & 0xFF000000) >> 24;
  packetBuffer[33] = (receiveTime.ntptime() & 0x00FF0000) >> 16;
  packetBuffer[34] = (receiveTime.ntptime() & 0x0000FF00) >> 8;
  packetBuffer[35] = (receiveTime.ntptime() & 0x000000FF);
  byte recCent[4];
  d2ba(1.0 * receiveTime.centisecond() / 100, recCent);
  packetBuffer[36] = recCent[0];
  packetBuffer[37] = recCent[1];
  packetBuffer[38] = recCent[2];
  packetBuffer[39] = recCent[3];

  // Transmit Time
  transmitTime = gps.getZDA();
  packetBuffer[40] = (transmitTime.ntptime() & 0xFF000000) >> 24;
  packetBuffer[41] = (transmitTime.ntptime() & 0x00FF0000) >> 16;
  packetBuffer[42] = (transmitTime.ntptime() & 0x0000FF00) >> 8;
  packetBuffer[43] = (transmitTime.ntptime() & 0x000000FF);
  byte traCent[4];
  d2ba(1.0 * transmitTime.centisecond() / 100, traCent);
  packetBuffer[44] = traCent[0];
  packetBuffer[45] = traCent[1];
  packetBuffer[46] = traCent[2];
  packetBuffer[47] = traCent[3];

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(remoteIP, remotePort);
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();

  Serial.println("End of UDP packet.");
}

/**
 * Double to byte array
 */
void d2ba(double cent, byte *output) {
  // Serial.println(cent);

  int j = 0;

  for (int i = 0; i < 8; i++){
    byte tmp = int(cent * 16);
    cent *= 16;
    cent = cent - floor(cent);
    // Serial.print(cent, 10);
    // Serial.print(", ");
    if (i % 2 == 1) {
      output[j] += tmp;
      j++;
    } else {
      output[j] = tmp * 16;
    }
  }

  // Serial.println("Debug loop");
  // for (int i = 0; i < 4; i++) {
  //   Serial.println(output[i]);
  // }
  // Serial.println("End debug loop");
}
