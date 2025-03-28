 
#include "defines.h"
#include <STM32RTC.h>
#include <NTPClient_Generic.h>                        // https://github.com/khoih-prog/NTPClient_Generic
#include <Wire.h>                                     // подключаем библиотеку для работы с шиной I2C
#include <iarduino_I2C_connect.h>                     // подключаем библиотеку для соединения arduino по шине I2C
#include <iarduino_GPS_NMEA.h>                        //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.
#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
#include <EEPROM.h>

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

// Вы можете указать пул сервера времени и смещение (в секундах, может быть
// изменено позже с помощью setTimeOffset() ). Дополнительно вы можете указать
// интервал обновления (в миллисекундах, может быть изменено с помощью setUpdateInterval() ).
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

  // Для других плат, при необходимости изменить
  #if ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
 // Необходимо использовать патч библиотеки для библиотек Ethernet, Ethernet2, EthernetLarge
    Ethernet.init (USE_THIS_SS_PIN);
   
  #elif USE_CUSTOM_ETHERNET
    // You have to add initialization for your Custom Ethernet here
    // This is just an example to setCSPin to USE_THIS_SS_PIN, and can be not correct and enough
    //Ethernet.init(USE_THIS_SS_PIN);
  
  #endif  //( ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
#endif

// запустить соединение Ethernet и сервер:
// Использовать динамический IP DHCP и случайный MAC
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

  gps.begin(SerialGPS);                           //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо аппаратной шины, можно указывать программную).
  SettingsGPS.begin(SerialGPS);                   //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).

  SettingsGPS.baudrate(9600);                   //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
  SettingsGPS.system(GPS_GP, GPS_GL);           //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
  SettingsGPS.composition(NMEA_RMC, NMEA_GSA, NMEA_GSV);  // Указываем что каждый пакет данных NMEA должен содержать только строки идентификаторов GSA(информация об активных спутниках) и GSV(информация о наблюдаемых спутниках).
  SettingsGPS.model(GPS_STATIC);                //  Указываем что модуль используется стационарно.
  SettingsGPS.updaterate(10);                   //  Указываем обновлять данные 10 раз в секунду. Функция gps.read() читает данные в 2 раза медленнее чем их выводит модуль.
  SettingsGPS.reset(GPS_COLD_START);            // Парамер функции reset()       указывающий стартовать с сохранением заданных настроек и сбросом данных о спутниках.
                                               
  //Wire.setClock(400000);                                // устанавливаем скорость передачи данных по шине I2C = 400кБит/с
  Wire.begin(0x01);                                       // инициируем подключение к шине I2C в качестве ведомого (slave) устройства, с указанием своего адреса на шине.
  I2CSlave.begin(REG_Array);                              // инициируем возможность чтения/записи данных по шине I2C, из/в указываемый массив

  Serial.print(F("\nStart Ethernet_NTPClient_Advanced_STM32 on "));// Serial.print(BOARD_NAME);
  Serial.print(F(" with ")); Serial.println(SHIELD_TYPE);
  Serial.println(ETHERNET_WEBSERVER_STM32_VERSION);
  Serial.println(NTPCLIENT_GENERIC_VERSION);

  initEthernet();

  timeClient.begin();

  Serial.println("Using NTP Server " + timeClient.getPoolServerName());

  //--------------------------------------------------------------------------------

  SerialUSB.println("We are waiting for the time GPS!");

 // getGpsTime(); // получаем время GPS

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
        //Serial.print("count : ");
        //Serial.println(count);
        count++;
        if (count == 999)count = 0;
    }
}

void timeSAT()
{
    gps.read();
                    //  Читаем данные.
    //   Выводим время:                                //
        Serial.print(gps.Hours); Serial.print(":"); //  Выводим час.
        Serial.print(gps.minutes); Serial.print(":"); //  Выводим минуты.
        Serial.print(gps.seconds); Serial.print(" "); //  Выводим секунды.
    //   Выводим дату:                                 //
        Serial.print(gps.day); Serial.print("."); //  Выводим день месяца.
        Serial.print(gps.month); Serial.print("."); //  Выводим месяц.
        Serial.print(gps.year + 2000); Serial.print(".");//  Выводим год.
    //   Выводим день недели:                          //
        Serial.print(" (");                           //
        Serial.print(wd[gps.weekday]);                //  Выводим день недели.
        Serial.print("), ");                          //
    //   Выводим количество секунд с начала эпохи Unix //
        Serial.print("UnixTime: ");                   //
        Serial.print(gps.Unix);                       //  Выводим время UnixTime.
        Serial.println("sec.");                           //

        newTimeGPS = true;
        //   Выводим информацию о наличии ошибок:          //
        if (gps.errTim)
        {
            newTimeGPS = false;
            Serial.print(" The time is unreliable.");     //  Выводим информацию о недостоверном времени.
        }                                             //
        if (gps.errDat)
        {
            newTimeGPS = false;
            Serial.print(" The date is unreliable.");      //  Выводим информацию о недостоверной дате.
        }                                             //
    //   Завершаем строку:                             //
        Serial.print("\r\n");                         //
}

void viewSattelite()
{
    gps.read(i);                                  //  Читаем данные с получением информации о спутниках в массив i (чтение может занимать больше 1 секунды). Если указать второй параметр gps.read(i,true); то в массиве окажутся данные только тех спутников, которые принимают участие в позиционировании.
    for (uint8_t j = 0; j < 30; j++) {                  //  Проходим по всем элементам массива «i».
        if (i[j][0]) {                             //  Если у спутника есть ID, то выводим информацию о нём:
            if (j < 9) Serial.print(0);               //  Выводим ведущий 0.
            Serial.print(j + 1);                     //  Выводим номер строки в мониторе последовательного порта.
            Serial.print(") Satellite ");            //
            Serial.print(sa[i[j][2]]);             //  Выводим название навигационной системы спутника.
            Serial.print(" ID: ");                 //
            if (i[j][0] < 10) Serial.print(0);        //  Выводим ведущий 0.
            Serial.print(i[j][0]);                 //  Выводим ID спутника.
            Serial.print(". Level: ");           //
            if (i[j][1] < 10) Serial.print(0);        //  Выводим ведущий 0.
            Serial.print(i[j][1]);                 //  Выводим отношение сигнал/шум (SNR) в дБ.
            Serial.print("dB. Elevation: ");      //
            if (i[j][4] < 10) Serial.print(0);        //  Выводим ведущий 0.
            Serial.print(i[j][4]);                 //  Выводим возвышение спутника относительно модуля (0°-горизонт ... 90°-зенит).
            Serial.print(". Azimuth: ");           //
            if (i[j][5] < 100) Serial.print(0);       //  Выводим ведущий 0.
            if (i[j][5] < 10) Serial.print(0);        //  Выводим ведущий 0.
            Serial.print(i[j][5] + i[j][6]);         //  Выводим азимут нахождения спутника относительно модуля (0...360°).
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
    bool     j = gps.read();                     //  Читаем сообщение NMEA получая флаг результата чтения.
    uint16_t k = gps.available;                  //  Получаем флаги наличия идентификаторов строк в прочитанном сообщении NMEA.
//   Проверяем наличие связи с GPS-модулем:       //
    if (!j) 
    {                                    //
        Serial.println("Data not read");   //  Если в мониторе последовательного порта появилось данное сообщение,
        newTimeGPS = false;
        return;                                  //  значит модуль не подключён или указана некорректная скорость UART.
    }                                            //
//   Выводим время выполнения чтения:             //
    Serial.print("Data read for ");        //
    Serial.print((float)(millis() - i) / 1000, 3);    //  Выводим время за которое функция read() прочитала сообщение NMEA.
    Serial.print(" seconds. ");                   //
//   Выводим состав прочитанного сообщения NMEA:  //
    Serial.print("The message contains the lines: "); //  Заполнение переменных функцией read() зависит от наличия соответствующих строк в сообщении NMEA.
    if (k & bit(0)) { Serial.print("GGA, "); }  //  Если Вы не видите требуемые Вам идентификаторы, настройте работу GPS-модуля, например, функцией 
    if (k & bit(1)) { Serial.print("GLL, "); }  //  composition() библиотеки iarduino_GPS_ATGM336 (если Вы пользуетесь GPS-модулем ATGM336).
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
//   Проверяем прочитано ли сообщение полностью:  //
    if (k & bit(15)) 
    {    
        newTimeGPS = true;//
        Serial.println("reading completed in full");//
    }
    else 
    { 
        newTimeGPS = false;//
        Serial.println("reading partially completed"); //  Если в мониторе последовательного порта появилось данное сообщение,
    }                                            //  то увеличте время выполнения чтения функцией timeOut().


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
    Serial.printf("Date %02d:%02d:%04d ", days, months, years + 2000);
    Serial.printf("Time %02d:%02d:%02d\n", hours, minutes, seconds);
}

String serStr; // строка для хранения пакетов от GPS приёмника
 
// Читает пакеты GPS приёмника из COM-порта и пытается найти в них время
// Если время найдено, возвращает True, иначе - False
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


// Декодирует вермя по NMEA пакету 
// и возвращает True в случае успеха и False в обратном случае
bool decodeTime(String s)
{
    if (s.substring(0, 6) == "$GNRMC")
    {
        String validFlag = s.substring(18, 19);
        // Ждём валидные данные (флаг "V" - данные не валидны, "A" - данные валидны):
        if (validFlag == "A")
        {
            String timeStr = s.substring(7, 17); // строка времени в формате ччммсс.сс
            // ищем индекс 4-ой запятой с конца, после которой идёт дата
            int commaIndex = 1;
            for (int i = 0; i < 6; i++)
            {
                commaIndex = s.lastIndexOf(",", commaIndex - 1);
            }
            String date = s.substring(commaIndex + 1, commaIndex + 7); // строка даты в формате ддммгг
 
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
        String SatStr = s.substring(3, 4); // строка количество спутников
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
            {                  //  Проходим по всем элементам массива «i».
                if (i[j][0]) {                             //  Если у спутника есть ID, то выводим информацию о нём:
                    if (j < 9) Serial.print(0);               //  Выводим ведущий 0.
                    Serial.print(j + 1);                     //  Выводим номер строки в мониторе последовательного порта.
                    Serial.print(") Спутник ");            //
                    Serial.print(sa[i[j][2]]);             //  Выводим название навигационной системы спутника.
                    Serial.print(" ID: ");                 //
                    if (i[j][0] < 10) Serial.print(0);        //  Выводим ведущий 0.
                    Serial.print(i[j][0]);                 //  Выводим ID спутника.
                    Serial.print(". Уровень: ");           //
                    if (i[j][1] < 10) Serial.print(0);        //  Выводим ведущий 0.
                    Serial.print(i[j][1]);                 //  Выводим отношение сигнал/шум (SNR) в дБ.
                    Serial.print("дБ. Возвышение: ");      //
                    if (i[j][4] < 10) Serial.print(0);        //  Выводим ведущий 0.
                    Serial.print(i[j][4]);                 //  Выводим возвышение спутника относительно модуля (0°-горизонт ... 90°-зенит).
                    Serial.print("°. Азимут: ");           //
                    if (i[j][5] < 100) Serial.print(0);       //  Выводим ведущий 0.
                    if (i[j][5] < 10) Serial.print(0);        //  Выводим ведущий 0.
                    Serial.print(i[j][5] + i[j][6]);         //  Выводим азимут нахождения спутника относительно модуля (0...360°).
                    Serial.print("°. ");                   //
                    if (i[j][3]) { Serial.print("Участвует в позиционировании."); }
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
            /* установить маркер A в 0 регистре modbus, это означает, что время готово к считыванию */
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