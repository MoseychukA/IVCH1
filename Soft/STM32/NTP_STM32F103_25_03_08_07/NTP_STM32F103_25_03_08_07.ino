 
#include "defines.h"
#include <STM32RTC.h>
#include <Wire.h>                                     // подключаем библиотеку для работы с шиной I2C
#include <iarduino_I2C_connect.h>                     // подключаем библиотеку для соединения arduino по шине I2C
#include <iarduino_GPS_NMEA.h>                        //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.
#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
#include <EEPROM.h>
#include <SPI.h>

#define debug true // для вывода отладочных сообщений

#define STARTOFTIME 2208988800UL

// Объявляем переменные и константы:
iarduino_I2C_connect I2CSlave;                    // объявляем объект I2C2 для работы c библиотекой iarduino_I2C_connect
byte           REG_Array[32];                    // объявляем массив из двух элементов, данные которого будут доступны мастеру (для чтения/записи) по шине I2C

#ifndef ONESECOND_IRQn
//#error "RTC has no feature for One-Second interrupt"
#endif

iarduino_GPS_NMEA    gps;                          //  Объявляем объект gps         для работы с функциями и методами библиотеки iarduino_GPS_NMEA.
iarduino_GPS_ATGM336 SettingsGPS;                  //  Объявляем объект SettingsGPS для работы с функциями и методами библиотеки iarduino_GPS_ATGM336.
uint8_t i[30][7];                                  //  Объявляем массив для получения данных о 30 спутниках в формате: {ID спутника (1...255), Отношение сигнал/шум (SNR) в дБ, тип навигационной системы (1-GPS/2-Глонасс/3-Galileo/4-Beidou/5-QZSS), Флаг участия спутника в позиционировании (1/0), Угол возвышения спутника (0°-горизонт ... 90°-зенит), Азимут положения спутника (0°...255°), Остаток азимута до 360° }.
char* sa[] = { "NoName ","GPS    ","GLONASS" };    //  Определяем массив строк содержащих названия навигационных систем спутников.

char* wd[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };   //  Определяем массив строк содержащих по две первых буквы из названий дня недели.


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

#define NTP_PORT 123 // стандартный порт, не менять

static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

int year;
byte month, day, hour, minute, second, hundredths;
unsigned long date, /*time,*/ age;
uint32_t timestamp, tempval;


#include <HardwareSerial.h>
//Hardware serial #1: RX = digital pin 10, TX = digital pin 11

HardwareSerial SerialRS2321(PA10, PA9);    //USART1
HardwareSerial SerialRS2322(PC11, PC10);   //USART4
HardwareSerial SerialGPS(PA3, PA2);        //USART2
HardwareSerial SerialRS4851(PB11, PB12);   //USART3
HardwareSerial SerialRS4852(PD2, PC12);    //USART5


/* Test diode*/
const int ledPin = PB5;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long previousMillisNTP = 0;
unsigned long previousMillisGPS = 0;
unsigned long previousMillisSAT = 0;
const long interval = 1000;               // interval at which to blink (milliseconds)
const long intervalNTP = 10000;           // interval at which to blink (milliseconds)
const long intervalGPS = 10000;
const long intervalSAT = 10000;
int count = 0;



void setup() 
{
 // Serial.begin(115200);
  SerialRS2321.begin(115200);
  SerialRS2322.begin(9600);
  SerialGPS.begin(9600);
  SerialRS4851.begin(9600);
  SerialRS4852.begin(9600);
 //// while (!Serial && millis() < 5000);
  delay(2000);
  SerialRS2321.println("Setup start! ");
 // Serial.println("Setup start! ");
  pinMode(ledPin, OUTPUT);
  //pinMode(PC4, OUTPUT);
  //digitalWrite(PC4, HIGH);
  gps.begin(SerialGPS);                                   //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо аппаратной шины, можно указывать программную).
  SettingsGPS.begin(SerialGPS);                           //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).

  SettingsGPS.baudrate(9600);                             //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
  SettingsGPS.system(GPS_GP, GPS_GL);                     //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
  SettingsGPS.composition(NMEA_RMC, NMEA_GSA, NMEA_GSV);  // Указываем что каждый пакет данных NMEA должен содержать только строки идентификаторов GSA(информация об активных спутниках) и GSV(информация о наблюдаемых спутниках).
  SettingsGPS.model(GPS_STATIC);                          //  Указываем что модуль используется стационарно.
  //SettingsGPS.updaterate(5);                            //  Указываем обновлять данные 10 раз в секунду. Функция gps.read() читает данные в 2 раза медленнее чем их выводит модуль.
  SettingsGPS.reset(GPS_COLD_START);                      // Парамер функции reset()       указывающий стартовать с сохранением заданных настроек и сбросом данных о спутниках.

  //Wire.setClock(400000);                                // устанавливаем скорость передачи данных по шине I2C = 400кБит/с
  Wire.begin(0x01);                                       // инициируем подключение к шине I2C в качестве ведомого (slave) устройства, с указанием своего адреса на шине.
  I2CSlave.begin(REG_Array);                              // инициируем возможность чтения/записи данных по шине I2C, из/в указываемый массив
  //Ethernet.setRstPin(PC4);
  //Ethernet.hardreset();
  // запускаем Ethernet шилд в режиме UDP:
  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP
  Ethernet.begin(mac[index], ip);
  // Ethernet.begin(mac[index]);
  SerialRS2321.print(F("Connected! IP address: "));
  SerialRS2321.println(Ethernet.localIP());
 
 
  Udp.begin(NTP_PORT);
  SerialRS2321.println("NTP started");

  SerialRS2321.println("RTC Init ");

  // Select RTC clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK.
  // By default the LSI is selected as source. Use LSE for better accuracy if available
  rtc.setClockSource(STM32RTC::LSE_CLOCK);

  // initialize RTC 24H format
  rtc.begin(hourFormat);
  SerialRS2321.println("Setup end! ");
 // Serial.println("Setup end! ");
}

void loop()
{
   processNTP(); // обрабатываем приходящие NTP запросы

   unsigned long currentMillis = millis();
   if (currentMillis - previousMillis >= interval)
   {
       // save the last time you blinked the LED
       previousMillis = currentMillis;
       // getRtcDate();

        // если светодиод не горит, включите его и наоборот:
       if (ledState == LOW)
       {
           ledState = HIGH;
       }
       else
       {
           ledState = LOW;
       }
       digitalWrite(ledPin, ledState); // установите светодиод с помощью ledState переменной:
       SerialRS2321.print("count : ");
       SerialRS2321.println(count);
       count++;
       if (count == 999)count = 0;
   }
}



// Обрабатывает запросы к NTP серверу
void processNTP()
{
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);
        IPAddress remote = Udp.remoteIP();
        int portNum = Udp.remotePort();

#if debug
        SerialRS2321.println();
        SerialRS2321.print("Received UDP packet size ");
        SerialRS2321.println(packetSize);
        SerialRS2321.print("From ");

        for (int i = 0; i < 4; i++)
        {
            SerialRS2321.print(remote[i], DEC);
            if (i < 3) { SerialRS2321.print("."); }
        }
        SerialRS2321.print(", port ");
        SerialRS2321.print(portNum);

        byte LIVNMODE = packetBuffer[0];
        SerialRS2321.print("  LI, Vers, Mode :");
        SerialRS2321.print(packetBuffer[0], HEX);

        byte STRATUM = packetBuffer[1];
        SerialRS2321.print("  Stratum :");
        SerialRS2321.print(packetBuffer[1], HEX);

        byte POLLING = packetBuffer[2];
        SerialRS2321.print("  Polling :");
        SerialRS2321.print(packetBuffer[2], HEX);

        byte PRECISION = packetBuffer[3];
        SerialRS2321.print("  Precision :");
        SerialRS2321.println(packetBuffer[3], HEX);

        for (int z = 0; z < NTP_PACKET_SIZE; z++) {
            SerialRS2321.print(packetBuffer[z], HEX);
            if (((z + 1) % 4) == 0) {
                SerialRS2321.println();
            }
        }
        SerialRS2321.println();
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

        getRtcDate();
        timestamp = numberOfSecondsSince1900Epoch(year, month, day, hour, minute, second);

#if debug
        SerialRS2321.println("Timestamp = " + (String)timestamp);
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
        Udp.beginPacket(remote, portNum);
        Udp.write(packetBuffer, NTP_PACKET_SIZE);
        Udp.endPacket();
    }
}

const uint8_t daysInMonth[] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 }; // число дней в месяцах
const unsigned long seventyYears = 2208988800UL; // перевод времени unix в эпоху

// Формирует метку времени от момента 01.01.1900
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

// Читает из RTC время и дату
void getRtcDate()
{
    rtc.getDate(&weekDays, &days, &months, &years);
    rtc.getTime(&hours, &minutes, &seconds, &subSeconds);

#if debug
    printDate();
#endif
}

// Выводит отформатированноую дату
void printDate()
{
    SerialRS2321.printf("Date %02d:%02d:%04d ", days, months, years + 2000);
    SerialRS2321.printf("Time %02d:%02d:%02d\n", hours, minutes, seconds);
}