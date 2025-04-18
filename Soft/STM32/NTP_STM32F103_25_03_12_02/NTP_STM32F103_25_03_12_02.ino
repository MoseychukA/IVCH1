 
#include "defines.h"
#include <STM32RTC.h>
#include <Wire.h>                                     // ���������� ���������� ��� ������ � ����� I2C
#include <iarduino_I2C_connect.h>                     // ���������� ���������� ��� ���������� arduino �� ���� I2C
#include <iarduino_GPS_NMEA.h>                        //  ���������� ���������� ��� ����������� ����� ��������� NMEA ���������� �� UART.
#include <iarduino_GPS_ATGM336.h>                    //  ���������� ���������� ��� ��������� ���������� ������ GPS ������ ATGM336.
#include <EEPROM.h>
#include <SPI.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789

#include <HardwareSerial.h>
//Hardware serial #1: RX = digital pin 10, TX = digital pin 11
HardwareSerial SerialRS2321(PA10, PA9);    //USART1
HardwareSerial SerialRS2322(PC11, PC10);   //USART4
HardwareSerial SerialGPS(PA3, PA2);        //USART2
HardwareSerial SerialRS4851(PB11, PB12);   //USART3
HardwareSerial SerialRS4852(PD2, PC12);    //USART5


// For the breakout board, you can use any 2 or 3 pins.
#define TFT_CS        PE7
#define TFT_DC        PE8
#define TFT_RST       PE9    // Or set to -1 and connect to board RESET pin
#define TFT_BL        PE10  // LED back-light

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
//#include <Fonts/FreeMonoBoldOblique12pt7b.h>
//#include <Fonts/FreeSerif9pt7b.h>
//#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSerifBoldItalic18pt7b.h>
#include <Fonts/FreeSerifBoldItalic9pt7b.h>
#include <Fonts/FreeSerifBoldItalic12pt7b.h>

#define debug true // ��� ������ ���������� ���������

#define STARTOFTIME 2208988800UL

// ��������� ���������� � ���������:
iarduino_I2C_connect I2CSlave;                    // ��������� ������ I2C2 ��� ������ c ����������� iarduino_I2C_connect
byte           REG_Array[32];                    // ��������� ������ �� ���� ���������, ������ �������� ����� �������� ������� (��� ������/������) �� ���� I2C

#ifndef ONESECOND_IRQn
//#error "RTC has no feature for One-Second interrupt"
#endif

iarduino_GPS_NMEA    gps;                          //  ��������� ������ gps         ��� ������ � ��������� � �������� ���������� iarduino_GPS_NMEA.
iarduino_GPS_ATGM336 SettingsGPS;                  //  ��������� ������ SettingsGPS ��� ������ � ��������� � �������� ���������� iarduino_GPS_ATGM336.
uint8_t i[30][7];                                  //  ��������� ������ ��� ��������� ������ � 30 ��������� � �������: {ID �������� (1...255), ��������� ������/��� (SNR) � ��, ��� ������������� ������� (1-GPS/2-�������/3-Galileo/4-Beidou/5-QZSS), ���� ������� �������� � ���������������� (1/0), ���� ���������� �������� (0�-�������� ... 90�-�����), ������ ��������� �������� (0�...255�), ������� ������� �� 360� }.
char* sa[] = { "NoName ","GPS    ","GLONASS" };    //  ���������� ������ ����� ���������� �������� ������������� ������ ���������.

char* wd[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };   //  ���������� ������ ����� ���������� �� ��� ������ ����� �� �������� ��� ������.


STM32RTC& rtc = STM32RTC::getInstance();

const byte weekDay_rtc = 7;
uint8_t weekDays = 0;
uint8_t days = 0;
uint8_t months = 0;
uint8_t years = 0;
uint8_t seconds = 0;
uint8_t minutes = 0;
uint8_t hours = 0;
uint32_t subSeconds = 0;

static STM32RTC::Hour_Format hourFormat = STM32RTC::HOUR_24;
static STM32RTC::AM_PM period = STM32RTC::AM;

static char lat_s = '0', lon_s = '0', qual = '0', stateGPS = '0', command = '0';
String lat, lon, date_GPS, sats, time_GPS;
boolean renew = false;
uint32_t unixTime_last_sync = 0, Time_last_sync = 0;

EthernetUDP Udp;

#define NTP_PORT 123 // ����������� ����, �� ������

static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

int year;
byte month, day, hour, minute, second, hundredths;
unsigned long date, /*time,*/ age;
uint32_t timestamp, tempval;

/* Test diode*/
const int ledPin = PB5;// RED LED the number of the LED pin
const int ledPin2 = PE5;// RED LED the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
int ledState2 = HIGH;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long previousMillisNTP = 0;
unsigned long previousMillisGPS = 0;
unsigned long previousMillisSAT = 0;
unsigned long previousMillisLED2 = 0;        // will store last time LED was updated
const long interval = 1000;               // interval at which to blink (milliseconds)
const long intervalNTP = 10000;           // interval at which to blink (milliseconds)
const long intervalGPS = 10000;
const long intervalSAT = 10000;
const long intervalLED = 700;
int count = 0;



void setup() 
{
  Serial.begin(115200);

  SerialRS2321.begin(9600);
  SerialRS2322.begin(9600);
  SerialGPS.begin(9600);
  SerialRS4851.begin(9600);
  SerialRS4852.begin(9600);
 
 // while (!Serial && millis() < 5000);
  delay(2000);

  SerialRS2321.println("Setup start! ");
  Serial.println("Setup start! ");

  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt + 1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);
 
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, HIGH); // ���������� ��������� 
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, LOW); // ���������� ��������� 
  gps.begin(SerialGPS);                                   //  ���������� ����������� ����� NMEA ������ ������ ������������ ���� UART (������ ���������� ����, ����� ��������� �����������).
  SettingsGPS.begin(SerialGPS);                           //  ���������� ������ � GPS ������� �� ��������� ���� UART. ������� ���� ��������� ������� �������� GPS ������ ATGM336 (������ ���������� ����, ����� ��������� �����������).

  SettingsGPS.baudrate(9600);                             //  ������������� �������� �������� ������ ������� � �������� ������ ���� Serial1 � 9600 ���/���.
  SettingsGPS.system(GPS_GP, GPS_GL);                     //  ��������� ��� ������ ����� �������� �� ��������� ������������� ������ GPS (GPS_GP) � Glonass (GPS_GL).
  SettingsGPS.composition(NMEA_RMC, NMEA_GSA, NMEA_GSV);  // ��������� ��� ������ ����� ������ NMEA ������ ��������� ������ ������ ��������������� GSA(���������� �� �������� ���������) � GSV(���������� � ����������� ���������).
  SettingsGPS.model(GPS_STATIC);                          //  ��������� ��� ������ ������������ �����������.
  //SettingsGPS.updaterate(10);                            //  ��������� ��������� ������ 10 ��� � �������. ������� gps.read() ������ ������ � 2 ���� ��������� ��� �� ������� ������.
  SettingsGPS.reset(GPS_COLD_START);                      // ������� ������� reset()       ����������� ���������� � ����������� �������� �������� � ������� ������ � ���������.

  //Wire.setClock(400000);                                // ������������� �������� �������� ������ �� ���� I2C = 400����/�
  Wire.begin(0x01);                                       // ���������� ����������� � ���� I2C � �������� �������� (slave) ����������, � ��������� ������ ������ �� ����.
  I2CSlave.begin(REG_Array);                              // ���������� ����������� ������/������ ������ �� ���� I2C, ��/� ����������� ������
  

  tft.init(170, 320, SPI_MODE2);           // Init ST7789 170x320
  tft.setRotation(1);
 
  tft.fillScreen(ST77XX_BLUE);
  tft.setTextSize(1);
  tft.setFont(&FreeSerifBoldItalic18pt7b);
  digitalWrite(TFT_BL, HIGH); // ���������� ���������
    
  const char EPD_SoftRF_text1[] = "(C) 2025";
  const char EPD_SoftRF_text2[] = "www.decima.ru";
  
  tft.setCursor(90, 50);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("NTP server");
  tft.setCursor(100, 100);
  tft.print("DECIMA");
  tft.setFont(&FreeSerifBoldItalic9pt7b);

  tft.setCursor(38, 140);
  tft.print(EPD_SoftRF_text1);
  tft.setFont(&FreeSerifBoldItalic12pt7b);
  tft.setCursor(120, 140);
  tft.print(EPD_SoftRF_text2);
  tft.setFont(&FreeSerifBoldItalic9pt7b);
  tft.setCursor(35, 162);
  tft.print(ver_soft);

  tft.setFont();
  // ��������� Ethernet ���� � ������ UDP:
  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP 
  //Ethernet.begin(mac[index], ip);
  Ethernet.begin(mac[index]);
  delay(5000);
  Serial.print(F("Connected! IP address: "));
  Serial.println(Ethernet.localIP());
 
 
  Udp.begin(NTP_PORT);
  Serial.println("NTP started");

  Serial.println("RTC Init ");

  // Select RTC clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK.
  // By default the LSI is selected as source. Use LSE for better accuracy if available
  rtc.setClockSource(STM32RTC::LSE_CLOCK);

  // initialize RTC 24H format
  rtc.begin(hourFormat);

  //tft.init(170, 320, SPI_MODE2);           // Init ST7789 170x320
  //tft.setRotation(1);
  ////digitalWrite(TFT_BL, HIGH); // ���������� ���������

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);
  testdrawtext("Connected! IP address:  ", ST77XX_WHITE);
  tft.setCursor(0, 20);
  tft.print(Ethernet.localIP());

  SerialRS2321.println("Setup end! ");
  Serial.println("Setup end! ");
}

void loop()
{
   processNTP(); // ������������ ���������� NTP �������. ���������� ����� � ���

   unsigned long currentMillis = millis();
   if (currentMillis - previousMillis >= interval)
   {
       // save the last time you blinked the LED
       previousMillis = currentMillis;
       // getRtcDate();

        // ���� ��������� �� �����, �������� ��� � ��������:
       if (ledState == LOW)
       {
           ledState = HIGH;
       }
       else
       {
           ledState = LOW;
       }
       digitalWrite(ledPin, ledState); // ���������� ��������� � ������� ledState ����������:
       //Serial.print("count : ");
       //Serial.println(count);
       count++;
       if (count == 999)count = 0;
   }

   if (ledState2 == LOW)
   {
       if (currentMillis - previousMillisLED2 >= intervalLED)
       {
           // save the last time you blinked the LED
           previousMillisLED2 = currentMillis;
           ledState2 = HIGH;
           digitalWrite(ledPin2, ledState2); // ���������� ��������� � ������� ledState ����������:
       }
   }
}



// ������������ ������� � NTP �������
void processNTP()
{
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
        ledState2 = LOW;             // ledState used to set the LED
        digitalWrite(ledPin2, ledState2); // ���������� ��������� � ������� ledState ����������:
        Udp.read(packetBuffer, NTP_PACKET_SIZE);
        IPAddress remote = Udp.remoteIP();
        int portNum = Udp.remotePort();

#if debug
        Serial.println();
        Serial.print("Received UDP packet size ");
        Serial.println(packetSize);
        Serial.print("From ");

        for (int i = 0; i < 4; i++)
        {
            Serial.print(remote[i], DEC);
            if (i < 3) { Serial.print("."); }
        }
        Serial.print(", port ");
        Serial.print(portNum);

        byte LIVNMODE = packetBuffer[0];
        Serial.print("  LI, Vers, Mode :");
        Serial.print(packetBuffer[0], HEX);

        byte STRATUM = packetBuffer[1];
        Serial.print("  Stratum :");
        Serial.print(packetBuffer[1], HEX);

        byte POLLING = packetBuffer[2];
        Serial.print("  Polling :");
        Serial.print(packetBuffer[2], HEX);

        byte PRECISION = packetBuffer[3];
        Serial.print("  Precision :");
        Serial.println(packetBuffer[3], HEX);

        for (int z = 0; z < NTP_PACKET_SIZE; z++) {
            Serial.print(packetBuffer[z], HEX);
            if (((z + 1) % 4) == 0) {
                Serial.println();
            }
        }
        Serial.println();
#endif

        // ����������� ������ � �������� �����:
        packetBuffer[0] = 0b00100100;   // ������, �����
        packetBuffer[1] = 1;   // �������
        packetBuffer[2] = 6;   // �������� ������
        packetBuffer[3] = 0xFA; // ��������

        packetBuffer[7] = 0; // ��������
        packetBuffer[8] = 0;
        packetBuffer[9] = 8;
        packetBuffer[10] = 0;

        packetBuffer[11] = 0; // ���������
        packetBuffer[12] = 0;
        packetBuffer[13] = 0xC;
        packetBuffer[14] = 0;

        getRtcDate();
        timestamp = numberOfSecondsSince1900Epoch(year, month, day, hour, minute, second);

#if debug
        Serial.println("Timestamp = " + (String)timestamp);
#endif

        tempval = timestamp;

        packetBuffer[12] = 71; //"G";
        packetBuffer[13] = 80; //"P";
        packetBuffer[14] = 83; //"S";
        packetBuffer[15] = 0; //"0";

        // ������������� ����� 
        packetBuffer[16] = (tempval >> 24) & 0xFF;
        tempval = timestamp;
        packetBuffer[17] = (tempval >> 16) & 0xFF;
        tempval = timestamp;
        packetBuffer[18] = (tempval >> 8) & 0xFF;
        tempval = timestamp;
        packetBuffer[19] = (tempval) & 0xFF;

        packetBuffer[20] = 0;
        packetBuffer[21] = 0;
        packetBuffer[22] = 0;
        packetBuffer[23] = 0;

        // �������� ����� ������� ������� 
        packetBuffer[24] = packetBuffer[40];
        packetBuffer[25] = packetBuffer[41];
        packetBuffer[26] = packetBuffer[42];
        packetBuffer[27] = packetBuffer[43];
        packetBuffer[28] = packetBuffer[44];
        packetBuffer[29] = packetBuffer[45];
        packetBuffer[30] = packetBuffer[46];
        packetBuffer[31] = packetBuffer[47];

        // ����� ������� 
        packetBuffer[32] = (tempval >> 24) & 0xFF;
        tempval = timestamp;
        packetBuffer[33] = (tempval >> 16) & 0xFF;
        tempval = timestamp;
        packetBuffer[34] = (tempval >> 8) & 0xFF;
        tempval = timestamp;
        packetBuffer[35] = (tempval) & 0xFF;

        packetBuffer[36] = 0;
        packetBuffer[37] = 0;
        packetBuffer[38] = 0;
        packetBuffer[39] = 0;

        // ���������� ����� ������� 
        packetBuffer[40] = (tempval >> 24) & 0xFF;
        tempval = timestamp;
        packetBuffer[41] = (tempval >> 16) & 0xFF;
        tempval = timestamp;
        packetBuffer[42] = (tempval >> 8) & 0xFF;
        tempval = timestamp;
        packetBuffer[43] = (tempval) & 0xFF;

        packetBuffer[44] = 0;
        packetBuffer[45] = 0;
        packetBuffer[46] = 0;
        packetBuffer[47] = 0;

        // ���������� NTP ����� 
        Udp.beginPacket(remote, portNum);
        Udp.write(packetBuffer, NTP_PACKET_SIZE);
        Udp.endPacket();
    }
}

const uint8_t daysInMonth[] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 }; // ����� ���� � �������
const unsigned long seventyYears = 2208988800UL; // ������� ������� unix � �����

// ��������� ����� ������� �� ������� 01.01.1900
static unsigned long int numberOfSecondsSince1900Epoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) {
    if (y >= 1970) { y -= 1970; }
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i) {
        days += pgm_read_byte(daysInMonth + i - 1);
    }
    if (m > 2 && y % 4 == 0) { ++days; }
    days += 365 * y + (y + 3) / 4 - 1;
    return days * 24L * 3600L + h * 3600L + mm * 60L + s + seventyYears;
}

//------------------------------------------------------

// ������ �� RTC ����� � ����
void getRtcDate()
{
    rtc.getDate(&weekDays, &days, &months, &years);
    rtc.getTime(&hours, &minutes, &seconds, &subSeconds);

#if debug
    printDate();
#endif
}

// ������� ������������������ ����
void printDate()
{
    Serial.printf("Date %02d:%02d:%04d ", days, months, years + 2000);
    Serial.printf("Time %02d:%02d:%02d\n", hours, minutes, seconds);
}

//=======================================================================================

void testdrawtext(char* text, uint16_t color) 
{
    tft.setCursor(0, 0);
    tft.setTextColor(color);
    tft.setTextWrap(true);
    tft.print(text);
}