 




#include "defines.h"
//#include <STM32RTC.h>
#include <NTPClient_Generic.h>                        // https://github.com/khoih-prog/NTPClient_Generic
//#include <Wire.h>                                     // подключаем библиотеку для работы с шиной I2C
//#include <iarduino_I2C_connect.h>                     // подключаем библиотеку для соединения arduino по шине I2C
//
//#define debug true // для вывода отладочных сообщений
//
//// Объявляем переменные и константы:
//iarduino_I2C_connect I2CSlave;                    // объявляем объект I2C2 для работы c библиотекой iarduino_I2C_connect
//byte           REG_Array[32];                    // объявляем массив из двух элементов, данные которого будут доступны мастеру (для чтения/записи) по шине I2C
//
//#ifndef ONESECOND_IRQn
////#error "RTC has no feature for One-Second interrupt"
//#endif
//
//
//
//STM32RTC& rtc = STM32RTC::getInstance();
//
//const byte weekDay_rtc = 7;
//uint8_t weekDays = 0;
//uint8_t days = 0;
//uint8_t months = 0;
//uint8_t years = 0;
//uint8_t secs = 0;
//uint8_t mns = 0;
//uint8_t hrs = 0;
//uint32_t subs = 0;
//
//static STM32RTC::Hour_Format hourFormat = STM32RTC::HOUR_24;
//static STM32RTC::AM_PM period = STM32RTC::AM;
//
//EthernetUDP ntpUDP;  // A UDP instance to let us send and receive packets over UDP
//
//
//#include <SoftwareSerial.h>
////software serial #1: RX = digital pin 10, TX = digital pin 11
//SoftwareSerial portRS232(PA1, PA0);
//SoftwareSerial portGPS(PA3, PA2);
//SoftwareSerial portRS485(PB11, PB12);
//
//
//// NTP server
////World
////char timeServer[] = "time.nist.gov";
//// Canada
//char timeServer[] = "0.ca.pool.ntp.org";
////char timeServer[] = "1.ca.pool.ntp.org";
////char timeServer[] = "2.ca.pool.ntp.org";
////char timeServer[] = "3.ca.pool.ntp.org";
//// Europe
////char timeServer[] = ""europe.pool.ntp.org";
//
//
//bool newTimeGPS = false;
//
//
//#define TIME_ZONE_OFFSET_HRS            (+3)
//#define NTP_UPDATE_INTERVAL_MS          60000L
//
//// Вы можете указать пул сервера времени и смещение (в секундах, может быть
//// изменено позже с помощью setTimeOffset() ). Дополнительно вы можете указать
//// интервал обновления (в миллисекундах, может быть изменено с помощью setUpdateInterval() ).
//NTPClient timeClient(ntpUDP, timeServer, (3600 * TIME_ZONE_OFFSET_HRS), NTP_UPDATE_INTERVAL_MS);
//
//
//void initEthernet()
//{
//#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)
//
//  ET_LOGWARN3(F("Board :"), BOARD_NAME, F(", setCsPin:"), USE_THIS_SS_PIN);
//
//  ET_LOGWARN(F("Default SPI pinout:"));
//  ET_LOGWARN1(F("MOSI:"), MOSI);
//  ET_LOGWARN1(F("MISO:"), MISO);
//  ET_LOGWARN1(F("SCK:"),  SCK);
//  ET_LOGWARN1(F("SS:"),   SS);
//  ET_LOGWARN(F("========================="));
//
//  // Для других плат, при необходимости изменить
//  #if ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
// // Необходимо использовать патч библиотеки для библиотек Ethernet, Ethernet2, EthernetLarge
//    Ethernet.init (USE_THIS_SS_PIN);
//   
//  #elif USE_CUSTOM_ETHERNET
//    // You have to add initialization for your Custom Ethernet here
//    // This is just an example to setCSPin to USE_THIS_SS_PIN, and can be not correct and enough
//    //Ethernet.init(USE_THIS_SS_PIN);
//  
//  #endif  //( ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
//#endif
//
//// запустить соединение Ethernet и сервер:
//// Использовать динамический IP DHCP и случайный MAC
//  uint16_t index = millis() % NUMBER_OF_MAC;
//  // Use Static IP
//  Ethernet.begin(mac[index], ip);
// // Ethernet.begin(mac[index]);
//
//  // you're connected now, so print out the data
//  Serial.print(F("You're connected to the network, IP = "));
//  Serial.println(Ethernet.localIP());  
//}
//
///* Test diode*/
//const int ledPin = 45;// the number of the LED pin
//int ledState = LOW;             // ledState used to set the LED
//unsigned long previousMillis = 0;        // will store last time LED was updated
//unsigned long previousMillisNTP = 0;
//const long interval = 1000;               // interval at which to blink (milliseconds)
//const long intervalNTP = 10000;           // interval at which to blink (milliseconds)
//int count = 0;
//


#define debug true // для вывода отладочных сообщений

//#include <SoftwareSerial.h>
#include <Wire.h>
#include <Ethernet3.h>
#include <EthernetUdp.h>
//#include <Ethernet_Generic.h>

//SoftwareSerial Serial1(10, 11);
EthernetUDP Udp;

// MAC, IP-адрес и порт NTP сервера:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // задайте свой MAC
IPAddress ip(192, 168, 1, 177); // задайте свой IP
#define NTP_PORT 123 // стандартный порт, не менять

#define RTC_ADDR 0x68 // i2c адрес RTC

static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

int year;
byte month, day, hour, minute, second, hundredths;
unsigned long date, time, age;
uint32_t timestamp, tempval;






void setup() 
{
  Serial.begin(115200);
  //portRS232.begin(9600);
  //portGPS.begin(9600);
  //portRS485.begin(9600);

  //while (!Serial && millis() < 5000);

  //pinMode(ledPin, OUTPUT);

  ////Wire.setClock(400000);                                // устанавливаем скорость передачи данных по шине I2C = 400кБит/с
  //Wire.begin(0x01);                                       // инициируем подключение к шине I2C в качестве ведомого (slave) устройства, с указанием своего адреса на шине.
  //I2CSlave.begin(REG_Array);                              // инициируем возможность чтения/записи данных по шине I2C, из/в указываемый массив

  //Serial.print(F("\nStart Ethernet_NTPClient_Advanced_STM32 on "));// Serial.print(BOARD_NAME);
  //Serial.print(F(" with ")); Serial.println(SHIELD_TYPE);
  //Serial.println(ETHERNET_WEBSERVER_STM32_VERSION);
  //Serial.println(NTPCLIENT_GENERIC_VERSION);

  initEthernet();

  timeClient.begin();

  Serial.println("Using NTP Server " + timeClient.getPoolServerName());

  ////--------------------------------------------------------------------------------

  //Serial.print("RTC Init ");

  //// Select RTC clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK.
  //// By default the LSI is selected as source. Use LSE for better accuracy if available
  // rtc.setClockSource(STM32RTC::LSE_CLOCK);

  //// initialize RTC 24H format
  //rtc.begin(hourFormat);


}

void loop()
{

    //unsigned long currentMillis = millis();

    //if (currentMillis - previousMillis >= interval)
    //{
    //    // save the last time you blinked the LED
    //    previousMillis = currentMillis;

    //    getRtcDate();


    //    if (newTimeGPS)
    //    {
    //        // SerialUSB.println("TimeGPS Ok!");
    //    }
    //    else
    //    {
    //        // SerialUSB.println("TimeGPS false!");

    //        if (currentMillis - previousMillisNTP >= intervalNTP)
    //        {
    //            // save the last time you blinked the LED
    //            previousMillisNTP = currentMillis;

    //            timeClient.update();

    //            if (timeClient.updated())
    //                Serial.println("********UPDATED********");
    //            else
    //                Serial.println("******NOT UPDATED******");

    //            Serial.println("UTC : " + timeClient.getFormattedUTCTime());
    //            Serial.println("UTC : " + timeClient.getFormattedUTCDateTime());
    //            Serial.println("LOC : " + timeClient.getFormattedTime());
    //            Serial.println("LOC : " + timeClient.getFormattedDateTime());
    //            Serial.println("UTC EPOCH : " + String(timeClient.getUTCEpochTime()));
    //            Serial.println("LOC EPOCH : " + String(timeClient.getEpochTime()));

    //            // Function test
    //            // Without leading 0
    //            Serial.println(String("UTC : ") + timeClient.getUTCHours() + ":" + timeClient.getUTCMinutes() + ":" + timeClient.getUTCSeconds() + " " +
    //                timeClient.getUTCDoW() + " " + timeClient.getUTCDay() + "/" + timeClient.getUTCMonth() + "/" + timeClient.getUTCYear() + " or " +
    //                timeClient.getUTCDay() + " " + timeClient.getUTCMonthStr() + " " + timeClient.getUTCYear());
    //            // With leading 0      
    //            Serial.println(String("UTC : ") + timeClient.getUTCStrHours() + ":" + timeClient.getUTCStrMinutes() + ":" + timeClient.getUTCStrSeconds() + " " +
    //                timeClient.getUTCDoW() + " " + timeClient.getUTCDay() + "/" + timeClient.getUTCMonth() + "/" + timeClient.getUTCYear() + " or " +
    //                timeClient.getUTCDay() + " " + timeClient.getUTCMonthStr() + " " + timeClient.getUTCYear());
    //            // Without leading 0
    //            Serial.println(String("LOC : ") + timeClient.getHours() + ":" + timeClient.getMinutes() + ":" + timeClient.getSeconds() + " " +
    //                timeClient.getDoW() + " " + timeClient.getDay() + "/" + timeClient.getMonth() + "/" + timeClient.getYear() + " or " +
    //                timeClient.getDay() + " " + timeClient.getMonthStr() + " " + timeClient.getYear());
    //            // With leading 0
    //            Serial.println(String("LOC : ") + timeClient.getStrHours() + ":" + timeClient.getStrMinutes() + ":" + timeClient.getStrSeconds() + " " +
    //                timeClient.getDoW() + " " + timeClient.getDay() + "/" + timeClient.getMonth() + "/" + timeClient.getYear() + " or " +
    //                timeClient.getDay() + " " + timeClient.getMonthStr() + " " + timeClient.getYear());

    //            rtc.setHours(timeClient.getHours(), period);
    //            rtc.setMinutes(timeClient.getMinutes());
    //            rtc.setSeconds(timeClient.getSeconds());

    //            // Set the date
    //            rtc.setWeekDay(weekDay_rtc);
    //            rtc.setDay(timeClient.getDay());
    //            rtc.setMonth(timeClient.getMonth());
    //            rtc.setYear(timeClient.getYear()-2000);

    //            rtc.getDate(&weekDays, &days, &months, &years);
    //            Serial.printf("\nDate %02d:%02d:%04d ", days, months, years+2000);
    //            rtc.getTime(&hrs, &mns, &secs, &subs);
    //            Serial.printf("Time %02d:%02d:%02d\n", hrs, mns, secs);
    //        }
    //    }

    //    // если светодиод не горит, включите его и наоборот:
    //    if (ledState == LOW)
    //    {
    //        ledState = HIGH;
    //    }
    //    else
    //    {
    //        ledState = LOW;
    //    }
    //    digitalWrite(ledPin, ledState); // установите светодиод с помощью ledState переменной:
    //    //Serial.print("count : ");
    //    //Serial.println(count);
    //    count++;
    //    if (count == 999)count = 0;
    //}
}

//------------------------------------------------------
/* callback function on each second interrupt */
//void rtc_SecondsCB(void* data)
//{
//    UNUSED(data);
//    toggling = !toggling;
//}

//// Читает из RTC время и дату
//void getRtcDate()
//{
//    rtc.getDate(&weekDays, &days, &months, &years);
//    rtc.getTime(&hrs, &mns, &secs, &subs);
//
//#if debug
//    printDate();
//#endif
//}
//
//// Выводит отформатированноую дату
//void printDate()
//{
//    Serial.printf("\nDate %02d:%02d:%04d ", days, months, years + 2000);
//    Serial.printf("Time %02d:%02d:%02d\n", hrs, mns, secs);
//}