 
#include "defines.h"
#include <STM32RTC.h>
#include <NTPClient_Generic.h>                        // https://github.com/khoih-prog/NTPClient_Generic
#include <Wire.h>                                     // ���������� ���������� ��� ������ � ����� I2C
#include <iarduino_I2C_connect.h>                     // ���������� ���������� ��� ���������� arduino �� ���� I2C
#include <iarduino_GPS_NMEA.h>                        //  ���������� ���������� ��� ����������� ����� ��������� NMEA ���������� �� UART.
#include <iarduino_GPS_ATGM336.h>                    //  ���������� ���������� ��� ��������� ���������� ������ GPS ������ ATGM336.
#include <EEPROM.h>

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


EthernetUDP ntpUDP;  // A UDP instance to let us send and receive packets over UDP

static char lat_s = '0', lon_s = '0', qual = '0', stateGPS = '0', command = '0';
String lat, lon, date_GPS, sats, time_GPS;
boolean renew = false;
uint32_t unixTime_last_sync = 0, Time_last_sync = 0;


#include <HardwareSerial.h>
//Hardware serial #1: RX = digital pin 10, TX = digital pin 11
HardwareSerial SerialRS232(PA10, PA9);
HardwareSerial SerialGPS(PA3, PA2);
HardwareSerial SerialRS485(PB11, PB12);

// NTP server
//World
//char timeServer[] = "time.nist.gov";
// Canada
char timeServer[] = "0.ca.pool.ntp.org";
//char timeServer[] = "1.ca.pool.ntp.org";
//char timeServer[] = "2.ca.pool.ntp.org";
//char timeServer[] = "3.ca.pool.ntp.org";
// Europe
//char timeServer[] = ""europe.pool.ntp.org";


//time_t tt;
//tm_t mtt;
uint8_t dateread[11];
bool dispflag = true;
bool newTimeGPS = false;


#define TIME_ZONE_OFFSET_HRS            (+3)
#define NTP_UPDATE_INTERVAL_MS          60000L

// �� ������ ������� ��� ������� ������� � �������� (� ��������, ����� ����
// �������� ����� � ������� setTimeOffset() ). ������������� �� ������ �������
// �������� ���������� (� �������������, ����� ���� �������� � ������� setUpdateInterval() ).
NTPClient timeClient(ntpUDP, timeServer, (3600 * TIME_ZONE_OFFSET_HRS), NTP_UPDATE_INTERVAL_MS);


void initEthernet()
{
#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)

 /* ET_LOGWARN3(F("Board :"), BOARD_NAME, F(", setCsPin:"), USE_THIS_SS_PIN);

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("MOSI:"), MOSI);
  ET_LOGWARN1(F("MISO:"), MISO);
  ET_LOGWARN1(F("SCK:"),  SCK);
  ET_LOGWARN1(F("SS:"),   SS);
  ET_LOGWARN(F("========================="));*/

  // ��� ������ ����, ��� ������������� ��������
  #if ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
 // ���������� ������������ ���� ���������� ��� ��������� Ethernet, Ethernet2, EthernetLarge
    Ethernet.init (USE_THIS_SS_PIN);
   
  #elif USE_CUSTOM_ETHERNET
    // You have to add initialization for your Custom Ethernet here
    // This is just an example to setCSPin to USE_THIS_SS_PIN, and can be not correct and enough
    //Ethernet.init(USE_THIS_SS_PIN);
  
  #endif  //( ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
#endif

// ��������� ���������� Ethernet � ������:
// ������������ ������������ IP DHCP � ��������� MAC
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP
  Ethernet.begin(mac[index], ip);
 // Ethernet.begin(mac[index]);

  // you're connected now, so print out the data
  Serial.print(F("You're connected to the network, IP = "));
  Serial.println(Ethernet.localIP());  
}

/* Test diode*/
const int ledPin = 45;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis    = 0;        // will store last time LED was updated
unsigned long previousMillisNTP = 0;
unsigned long previousMillisGPS = 0;
unsigned long previousMillisSAT = 0;
const long interval    = 1000;               // interval at which to blink (milliseconds)
const long intervalNTP = 10000;           // interval at which to blink (milliseconds)
const long intervalGPS = 10000;
const long intervalSAT = 10000;
int count = 0;


void setup() 
{
  Serial.begin(115200);

  SerialRS232.begin(9600);
  SerialGPS.begin(9600);
  SerialRS485.begin(9600);
  while (!Serial && millis() < 5000);

  pinMode(ledPin, OUTPUT);

  gps.begin(SerialGPS);                           //  ���������� ����������� ����� NMEA ������ ������ ������������ ���� UART (������ ���������� ����, ����� ��������� �����������).
  SettingsGPS.begin(SerialGPS);                   //  ���������� ������ � GPS ������� �� ��������� ���� UART. ������� ���� ��������� ������� �������� GPS ������ ATGM336 (������ ���������� ����, ����� ��������� �����������).

  SettingsGPS.baudrate(9600);                   //  ������������� �������� �������� ������ ������� � �������� ������ ���� Serial1 � 9600 ���/���.
  SettingsGPS.system(GPS_GP, GPS_GL);           //  ��������� ��� ������ ����� �������� �� ��������� ������������� ������ GPS (GPS_GP) � Glonass (GPS_GL).
  SettingsGPS.composition(NMEA_RMC, NMEA_GSA, NMEA_GSV);  // ��������� ��� ������ ����� ������ NMEA ������ ��������� ������ ������ ��������������� GSA(���������� �� �������� ���������) � GSV(���������� � ����������� ���������).
  SettingsGPS.model(GPS_STATIC);                //  ��������� ��� ������ ������������ �����������.
  SettingsGPS.updaterate(10);                   //  ��������� ��������� ������ 10 ��� � �������. ������� gps.read() ������ ������ � 2 ���� ��������� ��� �� ������� ������.
  SettingsGPS.reset(GPS_COLD_START);            // ������� ������� reset()       ����������� ���������� � ����������� �������� �������� � ������� ������ � ���������.
                                               
  //Wire.setClock(400000);                                // ������������� �������� �������� ������ �� ���� I2C = 400����/�
  Wire.begin(0x01);                                       // ���������� ����������� � ���� I2C � �������� �������� (slave) ����������, � ��������� ������ ������ �� ����.
  I2CSlave.begin(REG_Array);                              // ���������� ����������� ������/������ ������ �� ���� I2C, ��/� ����������� ������

  Serial.print(F("\nStart Ethernet_NTPClient_Advanced_STM32 on "));// Serial.print(BOARD_NAME);
  Serial.print(F(" with ")); Serial.println(SHIELD_TYPE);
  Serial.println(ETHERNET_WEBSERVER_STM32_VERSION);
  Serial.println(NTPCLIENT_GENERIC_VERSION);

  initEthernet();

  timeClient.begin();

  Serial.println("Using NTP Server " + timeClient.getPoolServerName());

  //--------------------------------------------------------------------------------

  SerialUSB.println("We are waiting for the time GPS!");

 // getGpsTime(); // �������� ����� GPS

  Serial.print("RTC Init ");

  // Select RTC clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK.
  // By default the LSI is selected as source. Use LSE for better accuracy if available
   rtc.setClockSource(STM32RTC::LSE_CLOCK);

  // initialize RTC 24H format
  rtc.begin(hourFormat);
}

void loop()
{
   // processGNRMC(&SerialGPS);
 
    unsigned long currentMillis = millis();
    if (!newTimeGPS)
    {
        if (currentMillis - previousMillisNTP >= intervalNTP)
        {
            previousMillisNTP = currentMillis;
            EthernetTime();
        }
    }

    if (currentMillis - previousMillisGPS >= intervalGPS)
    {
        previousMillisGPS = currentMillis;

        testGPS();
        if (newTimeGPS)
        {
            timeSAT();
        }
    }

    if (currentMillis - previousMillisSAT >= intervalSAT)
    {
        previousMillisSAT = currentMillis;
       // viewSattelite();
    }
 

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
}

void timeSAT()
{
    gps.read();
                    //  ������ ������.
    //   ������� �����:                                //
        Serial.print(gps.Hours); Serial.print(":"); //  ������� ���.
        Serial.print(gps.minutes); Serial.print(":"); //  ������� ������.
        Serial.print(gps.seconds); Serial.print(" "); //  ������� �������.
    //   ������� ����:                                 //
        Serial.print(gps.day); Serial.print("."); //  ������� ���� ������.
        Serial.print(gps.month); Serial.print("."); //  ������� �����.
        Serial.print(gps.year + 2000); Serial.print(".");//  ������� ���.
    //   ������� ���� ������:                          //
        Serial.print(" (");                           //
        Serial.print(wd[gps.weekday]);                //  ������� ���� ������.
        Serial.print("), ");                          //
    //   ������� ���������� ������ � ������ ����� Unix //
        Serial.print("UnixTime: ");                   //
        Serial.print(gps.Unix);                       //  ������� ����� UnixTime.
        Serial.println("sec.");                           //

        newTimeGPS = true;
        //   ������� ���������� � ������� ������:          //
        if (gps.errTim)
        {
            newTimeGPS = false;
            Serial.print(" The time is unreliable.");     //  ������� ���������� � ������������� �������.
        }                                             //
        if (gps.errDat)
        {
            newTimeGPS = false;
            Serial.print(" The date is unreliable.");      //  ������� ���������� � ������������� ����.
        }                                             //
    //   ��������� ������:                             //
        Serial.print("\r\n");                         //
}

void viewSattelite()
{
    gps.read(i);                                  //  ������ ������ � ���������� ���������� � ��������� � ������ i (������ ����� �������� ������ 1 �������). ���� ������� ������ �������� gps.read(i,true); �� � ������� �������� ������ ������ ��� ���������, ������� ��������� ������� � ����������������.
    for (uint8_t j = 0; j < 30; j++) {                  //  �������� �� ���� ��������� ������� �i�.
        if (i[j][0]) {                             //  ���� � �������� ���� ID, �� ������� ���������� � ��:
            if (j < 9) Serial.print(0);               //  ������� ������� 0.
            Serial.print(j + 1);                     //  ������� ����� ������ � �������� ����������������� �����.
            Serial.print(") Satellite ");            //
            Serial.print(sa[i[j][2]]);             //  ������� �������� ������������� ������� ��������.
            Serial.print(" ID: ");                 //
            if (i[j][0] < 10) Serial.print(0);        //  ������� ������� 0.
            Serial.print(i[j][0]);                 //  ������� ID ��������.
            Serial.print(". Level: ");           //
            if (i[j][1] < 10) Serial.print(0);        //  ������� ������� 0.
            Serial.print(i[j][1]);                 //  ������� ��������� ������/��� (SNR) � ��.
            Serial.print("dB. Elevation: ");      //
            if (i[j][4] < 10) Serial.print(0);        //  ������� ������� 0.
            Serial.print(i[j][4]);                 //  ������� ���������� �������� ������������ ������ (0�-�������� ... 90�-�����).
            Serial.print(". Azimuth: ");           //
            if (i[j][5] < 100) Serial.print(0);       //  ������� ������� 0.
            if (i[j][5] < 10) Serial.print(0);        //  ������� ������� 0.
            Serial.print(i[j][5] + i[j][6]);         //  ������� ������ ���������� �������� ������������ ������ (0...360�).
            Serial.print(". ");                   //
            if (i[j][3]) { Serial.print("Participates in positioning."); }
            Serial.print("\r\n");                  //
        }                                          //
    }                                              //
    Serial.println("---------------------------"); //
}

void testGPS()
{
    uint32_t i = millis();                       //
    bool     j = gps.read();                     //  ������ ��������� NMEA ������� ���� ���������� ������.
    uint16_t k = gps.available;                  //  �������� ����� ������� ��������������� ����� � ����������� ��������� NMEA.
//   ��������� ������� ����� � GPS-�������:       //
    if (!j) 
    {                                    //
        Serial.println("Data not read");   //  ���� � �������� ����������������� ����� ��������� ������ ���������,
        newTimeGPS = false;
        return;                                  //  ������ ������ �� ��������� ��� ������� ������������ �������� UART.
    }                                            //
//   ������� ����� ���������� ������:             //
    Serial.print("Data read for ");        //
    Serial.print((float)(millis() - i) / 1000, 3);    //  ������� ����� �� ������� ������� read() ��������� ��������� NMEA.
    Serial.print(" seconds. ");                   //
//   ������� ������ ������������ ��������� NMEA:  //
    Serial.print("The message contains the lines: "); //  ���������� ���������� �������� read() ������� �� ������� ��������������� ����� � ��������� NMEA.
    if (k & bit(0)) { Serial.print("GGA, "); }  //  ���� �� �� ������ ��������� ��� ��������������, ��������� ������ GPS-������, ��������, �������� 
    if (k & bit(1)) { Serial.print("GLL, "); }  //  composition() ���������� iarduino_GPS_ATGM336 (���� �� ����������� GPS-������� ATGM336).
    if (k & bit(2)) { Serial.print("RMC, "); }  //
    if (k & bit(3)) { Serial.print("VTG, "); }  //
    if (k & bit(4)) { Serial.print("ZDA, "); }  //
    if (k & bit(5)) { Serial.print("DHV, "); }  //
    if (k & bit(6)) { Serial.print("GST, "); }  //
    if (k & bit(7)) { Serial.print("GSA, "); }  //
    if (k & bit(8)) { Serial.print("GSA, "); }  //
    if (k & bit(9)) { Serial.print("GSA, "); }  //
    if (k & bit(10)) { Serial.print("GSV, "); }  //
    if (k & bit(11)) { Serial.print("TXT, "); }  //
//   ��������� ��������� �� ��������� ���������:  //
    if (k & bit(15)) 
    {    
        newTimeGPS = true;//
        Serial.println("reading completed in full");//
    }
    else 
    { 
        newTimeGPS = false;//
        Serial.println("reading partially completed"); //  ���� � �������� ����������������� ����� ��������� ������ ���������,
    }                                            //  �� �������� ����� ���������� ������ �������� timeOut().


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

String serStr; // ������ ��� �������� ������� �� GPS ��������
 
// ������ ������ GPS �������� �� COM-����� � �������� ����� � ��� �����
// ���� ����� �������, ���������� True, ����� - False
void getGpsTime()
{
    bool timeFound = false;

    while (!timeFound && millis() < 20000)
    {
        while (SerialGPS.available() > 0)
        {
            char c = SerialGPS.read();
            if (c != '\n')
            {
                serStr.concat(c);
            }
            else
            {
                timeFound = decodeTime(serStr);
                serStr = "";
            }
        }
    }
}


// ���������� ����� �� NMEA ������ 
// � ���������� True � ������ ������ � False � �������� ������
bool decodeTime(String s)
{
    if (s.substring(0, 6) == "$GNRMC")
    {
        String validFlag = s.substring(18, 19);
        // ��� �������� ������ (���� "V" - ������ �� �������, "A" - ������ �������):
        if (validFlag == "A")
        {
            String timeStr = s.substring(7, 17); // ������ ������� � ������� ������.��
            // ���� ������ 4-�� ������� � �����, ����� ������� ��� ����
            int commaIndex = 1;
            for (int i = 0; i < 6; i++)
            {
                commaIndex = s.lastIndexOf(",", commaIndex - 1);
            }
            String date = s.substring(commaIndex + 1, commaIndex + 7); // ������ ���� � ������� ������
 
            rtc.setHours(timeStr.substring(0, 2).toInt(), period);
            rtc.setMinutes(timeStr.substring(2, 4).toInt());
            rtc.setSeconds(timeStr.substring(4, 6).toInt());
            rtc.setSubSeconds(timeStr.substring(7, 10).toInt());

            // Set the date
            rtc.setWeekDay(weekDay_rtc);
            rtc.setDay(date.substring(0, 2).toInt());
            rtc.setMonth(date.substring(2, 4).toInt());
            rtc.setYear(date.substring(4, 6).toInt());

            Serial.println("Time GPS");
#if debug
            //getRtcDate();
            //printDate();
#endif
            newTimeGPS = true;
            return true;
        }
    }
    if(s.substring(0, 6) == "$GNGSV")
    {
        Serial.println(s);
        String SatStr = s.substring(3, 4); // ������ ���������� ���������
        Serial.println(SatStr);

    }
    newTimeGPS = false;
    return false;
}



String readValue(HardwareSerial* serial_pointer)
{
    String buffer;
    uint32_t micr = micros();
    while (1)
    {
        if (micros() - micr > 100000) return buffer;
        int b = serial_pointer->read();
        if ((char)b == 'V') return "";

        if ((('0' <= (char)b) &&
            ((char)b <= '9')) || ((char)b == '.')
            || (('A' <= (char)b) &&
                ((char)b <= 'Z')))
        {
            /* read all values containing numbers letters and dots */
            buffer += (char)b;
        }
        /*stop reading when encounter a comma */
        if ((char)b == ',') return buffer;
    }
}

void commandChangeIp(uint16_t ipAddress[5])
{
    if (ipAddress[0] > 255 || ipAddress[1] > 255 || ipAddress[2] > 255 || ipAddress[3] > 255 ||
        ipAddress[0] < 0 || ipAddress[1] < 0 || ipAddress[2] < 0 || ipAddress[3] < 0)
    {
        Serial.println("Wrong IP in change ip command \n");
        return;
    }
    if ((ipAddress[0] + ipAddress[1] + ipAddress[2] + ipAddress[3] == ipAddress[4]) && ipAddress[4] != 0)
    {

        EEPROM.write(0, (uint8_t)ipAddress[0]);
        EEPROM.write(1, (uint8_t)ipAddress[1]);
        EEPROM.write(2, (uint8_t)ipAddress[2]);
        EEPROM.write(3, (uint8_t)ipAddress[3]);

        EEPROM.write(4, (uint8_t)(ipAddress[4] & 0xFF));
        EEPROM.write(5, (uint8_t)(ipAddress[4] >> 8));

        Serial.print("\nNew IP is: " + String(ipAddress[0]) + '.' + String(ipAddress[1]) + '.' + String(ipAddress[2]) + '.' + String(ipAddress[3]) + '\n');
        Serial.print("Rebooting \n");
        for (;;);
    }
    else
    {
        Serial.println("Invalid IP Checksum \n");
        Serial.println("Correct format example: 192,168,0,121,481, \n");
        Serial.println("192+168+0+121 = 481");
        return;
    }
    return;
}

void check_for_serial_commands(HardwareSerial* serial_pointer)
{
    if (serial_pointer->available() > 0)
    {
        uint16_t address1, address2, address3, address4, chksum;
        if (Serial.read() == '#' && Serial.read() == 'c'
            && Serial.read() == 'h' && Serial.read() == 'i'
            && Serial.read() == 'p')
        {
            address1 = atoi(readValue((HardwareSerial*)&Serial).c_str());
            address2 = atoi(readValue((HardwareSerial*)&Serial).c_str());
            address3 = atoi(readValue((HardwareSerial*)&Serial).c_str());
            address4 = atoi(readValue((HardwareSerial*)&Serial).c_str());
            chksum = atoi(readValue((HardwareSerial*)&Serial).c_str());
            uint16_t ipAddress[5] = { address1, address2, address3, address4, chksum };
            commandChangeIp(ipAddress);
        }
    }
}


void processGNRMC(HardwareSerial* serial_pointer)
{
    //$GLGSV
    if (serial_pointer->available() > 0)
    {
        char b = serial_pointer->read();
        switch (stateGPS)
        {
        case '0': if ('$' == b) stateGPS = '1'; else stateGPS = '0'; break;

        case '1': if ('G' == b) stateGPS = '2'; else stateGPS = '0'; break;

        case '2': if (('P' == b) || ('N' == b)) stateGPS = '3'; else stateGPS = '0'; break;

        case '3': 
            if ('R' == b)
            {
                stateGPS = '4';
            }
            else if ('G' == b)
            {
                stateGPS = '7';
            }
            else stateGPS = '0'; break;

        case '4': if ('M' == b) stateGPS = '5'; else stateGPS = '0'; break;

        case '5': if ('C' == b) stateGPS = '6'; else stateGPS = '0'; break;

            /* found $GPRMC/GNRMC stateGPSment */
        case '6':
            if (',' == b)
            {
                time_GPS = readValue(serial_pointer);
                lat = readValue(serial_pointer);
                lat.replace('.', ',');
                lat_s = (char)readValue(serial_pointer)[0];
                lon = readValue(serial_pointer);
                lon.replace('.', ',');
                lon_s = (char)readValue(serial_pointer)[0];
                qual = (char)readValue(serial_pointer)[0];
                sats = readValue(serial_pointer);
                readValue(serial_pointer); // skip empty
                date_GPS = readValue(serial_pointer);
               // Serial.print("time_GPS = " + time_GPS + " lat = " + lat + " lat_s =" + lat_s + " lon = " + lon + " lon_s = " + lon_s  + " qual = " +qual + " sats = " + sats + " date_GPS = " + date_GPS + "\r\n");
                renew = true;
                stateGPS = '0';
            }
            else stateGPS = '0'; break;
        case '7': if ('S' == b) stateGPS = '8'; else stateGPS = '0'; break;

        case '8': if ('V' == b) stateGPS = '9'; else stateGPS = '0'; break;

        case '9': 

            for (uint8_t j = 0; j < 30; j++) 
            {                  //  �������� �� ���� ��������� ������� �i�.
                if (i[j][0]) {                             //  ���� � �������� ���� ID, �� ������� ���������� � ��:
                    if (j < 9) Serial.print(0);               //  ������� ������� 0.
                    Serial.print(j + 1);                     //  ������� ����� ������ � �������� ����������������� �����.
                    Serial.print(") ������� ");            //
                    Serial.print(sa[i[j][2]]);             //  ������� �������� ������������� ������� ��������.
                    Serial.print(" ID: ");                 //
                    if (i[j][0] < 10) Serial.print(0);        //  ������� ������� 0.
                    Serial.print(i[j][0]);                 //  ������� ID ��������.
                    Serial.print(". �������: ");           //
                    if (i[j][1] < 10) Serial.print(0);        //  ������� ������� 0.
                    Serial.print(i[j][1]);                 //  ������� ��������� ������/��� (SNR) � ��.
                    Serial.print("��. ����������: ");      //
                    if (i[j][4] < 10) Serial.print(0);        //  ������� ������� 0.
                    Serial.print(i[j][4]);                 //  ������� ���������� �������� ������������ ������ (0�-�������� ... 90�-�����).
                    Serial.print("�. ������: ");           //
                    if (i[j][5] < 100) Serial.print(0);       //  ������� ������� 0.
                    if (i[j][5] < 10) Serial.print(0);        //  ������� ������� 0.
                    Serial.print(i[j][5] + i[j][6]);         //  ������� ������ ���������� �������� ������������ ������ (0...360�).
                    Serial.print("�. ");                   //
                    if (i[j][3]) { Serial.print("��������� � ����������������."); }
                    Serial.print("\r\n");                  //
                }                                          //
            }                                              //
            Serial.println("---------------------------"); //
           // Serial.print("Sat_GPS = ");
            stateGPS = '0'; 
            break;

        default: stateGPS = '0'; break;
        }

        /* if found flag A, we sync every 300 seconds todo : setup a stack with last 5 values from gps */
        if (renew && lat[0] == 'A' && (rtc.getEpoch() - Time_last_sync) > 10)
        {
            int hours = 0, minutes = 0, seconds = 0, days = 0, months = 0, years = 0;
            hours += (time_GPS[0] - '0') * 10;
            hours += time_GPS[1] - '0';
            minutes += (time_GPS[2] - '0') * 10;
            minutes += time_GPS[3] - '0';
            seconds += (time_GPS[4] - '0') * 10;
            seconds += time_GPS[5] - '0';

            days += (date_GPS[0] - '0') * 10;
            days += date_GPS[1] - '0';
            months += (date_GPS[2] - '0') * 10;
            months += date_GPS[3] - '0';
            years += (date_GPS[4] - '0') * 10;
            years += date_GPS[5] - '0';

            /* set internal time with time received from gps */
            set_Internal_time_GPS(hours, minutes, seconds, days, months, years);

            Serial.print("\r\ntime_GPS = " + time_GPS + " date_GPS = " + date_GPS + "\r\n");
            rtc.setTime(hours, minutes, seconds, subSeconds = 1000, period);
            renew = false;
            unixTime_last_sync = htonl(rtc.getEpoch() + STARTOFTIME);
            Time_last_sync = rtc.getEpoch();
 
            newTimeGPS = true;
            /* ���������� ������ A � 0 �������� modbus, ��� ��������, ��� ����� ������ � ���������� */
          //  mb.Hreg(mb_begin_offset + 0, 'A');
        }
        /* set marker V in 0 register of modbus, - time is NOT valid */
        else if ((rtc.getEpoch() - Time_last_sync) > 25)
        {
            // mb.Hreg(mb_begin_offset + 0, 'V');
            newTimeGPS = false;
        }
    }
}

void set_Internal_time_GPS(uint8_t hr, uint8_t min, uint8_t sec, uint8_t day, uint8_t mnth, int yr)
{
    uint32_t gps_time = rtc.getEpoch();/*{ (uint8_t)(yr - 1970), mnth, day, 0, 0, hr, min, sec };*/
    rtc.setEpoch(gps_time);
}

void EthernetTime()
{
    timeClient.update();

    if (timeClient.updated())
        Serial.println("********UPDATED********");
    else
        Serial.println("******NOT UPDATED******");

    Serial.println("UTC : " + timeClient.getFormattedUTCTime());
    Serial.println("UTC : " + timeClient.getFormattedUTCDateTime());
    Serial.println("LOC : " + timeClient.getFormattedTime());
    Serial.println("LOC : " + timeClient.getFormattedDateTime());
    Serial.println("UTC EPOCH : " + String(timeClient.getUTCEpochTime()));
    Serial.println("LOC EPOCH : " + String(timeClient.getEpochTime()));

    // Function test
    // Without leading 0
    Serial.println(String("UTC : ") + timeClient.getUTCHours() + ":" + timeClient.getUTCMinutes() + ":" + timeClient.getUTCSeconds() + " " +
        timeClient.getUTCDoW() + " " + timeClient.getUTCDay() + "/" + timeClient.getUTCMonth() + "/" + timeClient.getUTCYear() + " or " +
        timeClient.getUTCDay() + " " + timeClient.getUTCMonthStr() + " " + timeClient.getUTCYear());
    // With leading 0      
    Serial.println(String("UTC : ") + timeClient.getUTCStrHours() + ":" + timeClient.getUTCStrMinutes() + ":" + timeClient.getUTCStrSeconds() + " " +
        timeClient.getUTCDoW() + " " + timeClient.getUTCDay() + "/" + timeClient.getUTCMonth() + "/" + timeClient.getUTCYear() + " or " +
        timeClient.getUTCDay() + " " + timeClient.getUTCMonthStr() + " " + timeClient.getUTCYear());
    // Without leading 0
    Serial.println(String("LOC : ") + timeClient.getHours() + ":" + timeClient.getMinutes() + ":" + timeClient.getSeconds() + " " +
        timeClient.getDoW() + " " + timeClient.getDay() + "/" + timeClient.getMonth() + "/" + timeClient.getYear() + " or " +
        timeClient.getDay() + " " + timeClient.getMonthStr() + " " + timeClient.getYear());
    // With leading 0
    Serial.println(String("LOC : ") + timeClient.getStrHours() + ":" + timeClient.getStrMinutes() + ":" + timeClient.getStrSeconds() + " " +
        timeClient.getDoW() + " " + timeClient.getDay() + "/" + timeClient.getMonth() + "/" + timeClient.getYear() + " or " +
        timeClient.getDay() + " " + timeClient.getMonthStr() + " " + timeClient.getYear());

    rtc.setHours(timeClient.getHours(), period);
    rtc.setMinutes(timeClient.getMinutes());
    rtc.setSeconds(timeClient.getSeconds());

    // Set the date
    rtc.setWeekDay(weekDay_rtc);
    rtc.setDay(timeClient.getDay());
    rtc.setMonth(timeClient.getMonth());
    rtc.setYear(timeClient.getYear() - 2000);

    rtc.getDate(&weekDays, &days, &months, &years);
    Serial.printf("Date %02d:%02d:%04d ", days, months, years + 2000);
    rtc.getTime(&hours, &minutes, &seconds, &subSeconds);
    Serial.printf("Time %02d:%02d:%02d\n", hours, minutes, seconds);

}