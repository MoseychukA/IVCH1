/****************************************************************************************************************************
  BI_RTC_Ethernet_NTPClient_STM32.ino

 *****************************************************************************************************************************/
 
#include "defines.h"
#include <SoftwareSerial.h>
 //software serial #1: RX = digital pin 10, TX = digital pin 11
 //SoftwareSerial portOne(10, 11);


SoftwareSerial portRS232(PA10, PA9);
SoftwareSerial portGPS(PA3, PA2);
SoftwareSerial portRS485(PB11, PB12);

#define debug true // для вывода отладочных сообщений

//#include <Ethernet_STM.h>
#include <EthernetUdp.h>

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include <Timezone_Generic.h>             // https://github.com/khoih-prog/Timezone_Generic

#include <STM32RTC.h>                     // https://github.com/stm32duino/STM32RTC

/* Get the rtc object */
STM32RTC& rtc = STM32RTC::getInstance();



//////////////////////////////////////////

// US Eastern Time Zone (New York, Detroit)
TimeChangeRule myDST = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
TimeChangeRule mySTD = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours
Timezone myTZ(myDST, mySTD);

// If TimeChangeRules are already stored in EEPROM, comment out the three
// lines above and uncomment the line below.
//Timezone myTZ(100);       //assumes rules stored at EEPROM address 100

TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev

//////////////////////////////////////////

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include <NTPClient_Generic.h>          // https://github.com/khoih-prog/NTPClient_Generic

// A UDP instance to let us send and receive packets over UDP
EthernetUDP ntpUDP;
#define NTP_PORT 123 // стандартный порт, не менять

//static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];
//int year;
byte /*month, day, hour, minute, second,*/ hundredths;
uint32_t timestamp, tempval;


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

#define TIME_ZONE_OFFSET_HRS            (-3)
#define NTP_UPDATE_INTERVAL_MS          60000L

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, timeServer, (3600 * TIME_ZONE_OFFSET_HRS), NTP_UPDATE_INTERVAL_MS);

static bool gotCurrentTime = false;

// format and print a time_t value, with a time zone appended.
void printDateTime(time_t t, const char *tz)
{
  char buf[32];
  char m[4];    // temporary storage for month string (DateStrings.cpp uses shared buffer)
  strcpy(m, monthShortStr(month(t)));
  sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s",
          hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t), m, year(t), tz);
  Serial.println(buf);
}

void update_RTC(void)
{
  // Just get the correct time once
  if (timeClient.updated())
  {
    // Update RTC
    Serial.println("\nUpdating Time for STM32 RTC");
    Serial.println("********UPDATED********");
  
    // STM32 RTC clock starts from 01/01/2000 if not set. No battery-backed RTC.
    // STM32 RTC specific code
    rtc.setEpoch(timeClient.getUTCEpochTime());
    
    gotCurrentTime = true;
  }
}

void displayRTC()
{
  // Display time from RTC
  Serial.println("============================");

  // STM32 RTC specific code
  time_t utc = rtc.getEpoch();
  time_t local = myTZ.toLocal(utc, &tcr);
  //////
  
  printDateTime(utc, "UTC");
  printDateTime(local, tcr -> abbrev);
}

void check_status(void)
{
  // Update first time after 5s
  static ulong checkstatus_timeout  = 5000L;
  static ulong RTCDisplay_timeout   = 0;

  static ulong current_millis;

// Display every 10s
#define RTC_DISPLAY_INTERVAL        (10000L)

// Update RTC every hour if got correct time from NTP
#define RTC_UPDATE_INTERVAL         (3600 * 1000L)

// Retry updating RTC every 5s if haven't got correct time from NTP
#define RTC_RETRY_INTERVAL          (3 * 1000L)

  current_millis = millis();

  if ((current_millis > RTCDisplay_timeout) || (RTCDisplay_timeout == 0))
  {
    if (gotCurrentTime)
      displayRTC();
      
    RTCDisplay_timeout = current_millis + RTC_DISPLAY_INTERVAL;
  }
  
  // Update RTC every RTC_UPDATE_INTERVAL (3600) seconds.
  if ((current_millis > checkstatus_timeout))
  {
    update_RTC();
    
    if (gotCurrentTime)
    {
      Serial.println("RTC updated. Next update in seconds : " + String(RTC_UPDATE_INTERVAL/1000, DEC));
      checkstatus_timeout = current_millis + RTC_UPDATE_INTERVAL;
    }
    else
    {
      Serial.println("Retry RTC update in seconds : " + String(RTC_RETRY_INTERVAL/1000, DEC));
      checkstatus_timeout = current_millis + RTC_RETRY_INTERVAL;
    }   
  }
}

//////////////////////////////////////////

void initEthernet()
{
#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)

  ET_LOGWARN3(F("Board :"), BOARD_NAME, F(", setCsPin:"), USE_THIS_SS_PIN);

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("MOSI:"), MOSI);
  ET_LOGWARN1(F("MISO:"), MISO);
  ET_LOGWARN1(F("SCK:"),  SCK);
  ET_LOGWARN1(F("SS:"),   SS);
  ET_LOGWARN(F("========================="));

  // For other boards, to change if necessary
  #if ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
    // Must use library patch for Ethernet, Ethernet2, EthernetLarge libraries
    Ethernet.init (USE_THIS_SS_PIN);
   
  #elif USE_CUSTOM_ETHERNET
    // You have to add initialization for your Custom Ethernet here
    // This is just an example to setCSPin to USE_THIS_SS_PIN, and can be not correct and enough
    //Ethernet.init(USE_THIS_SS_PIN);
  
  #endif  //( ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
#endif

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP
  Ethernet.begin(mac[index], ip);
 // Ethernet.begin(mac[index]);

  // you're connected now, so print out the data
  Serial.print(F("You're connected to the network, IP = "));
  Serial.println(Ethernet.localIP());  
}

void setup()
{
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  portRS232.begin(9600);
  portGPS.begin(9600);
  portRS485.begin(9600);
  delay(2000);

  Serial.print(F("\nStart BI_RTC_Ethernet_NTPClient_STM32 on ")); Serial.print(BOARD_NAME);
  Serial.print(F(" with ")); Serial.println(SHIELD_TYPE);
  Serial.println(ETHERNET_WEBSERVER_STM32_VERSION);
  Serial.println(NTPCLIENT_GENERIC_VERSION);

  initEthernet();
  ntpUDP.begin(NTP_PORT);

  timeClient.begin();

  rtc.setClockSource(STM32RTC::LSE_CLOCK);   // Кварц 32768
  rtc.begin(); // initialize RTC 24H format
 

}

void loop()
{
  processNTP();                                           // обрабатываем приходящие NTP запросы

  timeClient.update();
  
  check_status();
}


// Обрабатывает запросы к NTP серверу
void processNTP()
{
    int packetSize = ntpUDP.parsePacket();
    if (packetSize)
    {
        ntpUDP.read(packetBuffer, NTP_PACKET_SIZE);
        IPAddress remote = ntpUDP.remoteIP();
        int portNum = ntpUDP.remotePort();

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

        for (int z = 0; z < NTP_PACKET_SIZE; z++)
        {
            Serial.print(packetBuffer[z], HEX);
            if (((z + 1) % 4) == 0)
            {
                Serial.println();
            }
        }
        Serial.println();
#endif

        // Упаковываем данные в ответный пакет:
        packetBuffer[0] = 0b00100100;   // версия, режим
        packetBuffer[1] = 1;   // стратум
        packetBuffer[2] = 6;   // интервал опроса
        packetBuffer[3] = 0xFA; // точность

        packetBuffer[7] = 0; // задержка
        packetBuffer[8] = 0;
        packetBuffer[9] = 8;
        packetBuffer[10] = 0;

        packetBuffer[11] = 0; // дисперсия
        packetBuffer[12] = 0;
        packetBuffer[13] = 0xC;
        packetBuffer[14] = 0;

        update_RTC();
       // getRtcDate();

       // timestamp = numberOfSecondsSince1900Epoch(year, month, day, hour, minute, second);

#if debug
        Serial.println("Timestamp = " + (String)timestamp);
#endif

        tempval = timestamp;

        packetBuffer[12] = 71; //"G";
        packetBuffer[13] = 80; //"P";
        packetBuffer[14] = 83; //"S";
        packetBuffer[15] = 0; //"0";

        // Относительное время 
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

        // Копируем метку времени клиента 
        packetBuffer[24] = packetBuffer[40];
        packetBuffer[25] = packetBuffer[41];
        packetBuffer[26] = packetBuffer[42];
        packetBuffer[27] = packetBuffer[43];
        packetBuffer[28] = packetBuffer[44];
        packetBuffer[29] = packetBuffer[45];
        packetBuffer[30] = packetBuffer[46];
        packetBuffer[31] = packetBuffer[47];

        // Метка времени 
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

        // Записываем метку времени 
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

        // Отправляем NTP ответ 
        ntpUDP.beginPacket(remote, portNum);
        ntpUDP.write(packetBuffer, NTP_PACKET_SIZE);
        ntpUDP.endPacket();
    }
}


// Выводит отформатированноую дату
//void printDate()
//{
//    char sz[32];
//    sprintf(sz, "Date %02d.%02d.%04d %02d:%02d:%02d.%03d", day, month, year, hour, minute, second, hundredths);
//    Serial.println(sz);
//}

const uint8_t daysInMonth[] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 }; // число дней в месяцах
const unsigned long seventyYears = 2208988800UL; // перевод времени unix в эпоху

// Формирует метку времени от момента 01.01.1900
static unsigned long int numberOfSecondsSince1900Epoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s)
{
    if (y >= 1970) { y -= 1970; }
    uint16_t days = d;
    for (uint8_t i = 1; i < m; ++i)
    {
        days += pgm_read_byte(daysInMonth + i - 1);
    }
    if (m > 2 && y % 4 == 0) { ++days; }
    days += 365 * y + (y + 3) / 4 - 1;
    return days * 24L * 3600L + h * 3600L + mm * 60L + s + seventyYears;
}
