#include <SPI.h>
#include <Ethernet_STM.h>
#include <EthernetUdp.h>
#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
#include <RTClock.h>





#define debug true // для вывода отладочных сообщений


//iarduino_GPS_NMEA    gps;                          //  Объявляем объект gps         для работы с функциями и методами библиотеки iarduino_GPS_NMEA.
iarduino_GPS_ATGM336 SettingsGPS;                  //  Объявляем объект SettingsGPS для работы с функциями и методами библиотеки iarduino_GPS_ATGM336.

EthernetUDP Udp;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
#if defined(WIZ550io_WITH_MACADDRESS) // Use assigned MAC address of WIZ550io
;
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#endif  

IPAddress ip(192, 168, 1, 177); // задайте свой IP
#define NTP_PORT 123 // стандартный порт, не менять

//#define RTC_ADDR 0x68 // i2c адрес RTC

static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

int year;
byte month, day, hour, minute, second, hundredths;
uint32_t timestamp, tempval;
 
RTClock rtclock(RTCSEL_LSE); // initialise
//int timezone = 0;      // change to your timezone
time_t tt;
tm_t mtt;
uint8_t dateread[11];
bool dispflag = true;


//-----------------------------------------------------------------------------
const char* weekdays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
const char* months[] = { "Dummy", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
//-----------------------------------------------------------------------------
uint8_t str2month(const char* d)
{
    uint8_t i = 13;
    while ((--i) && strcmp(months[i], d) != 0);
    return i;
} 
//-----------------------------------------------------------------------------
const char* delim = " :";
char s[128]; // for sprintf
//-----------------------------------------------------------------------------
void ParseBuildTimestamp(tm_t& mt)
{
    // Timestamp format: "Dec  8 2017, 22:57:54"
    sprintf(s, "Timestamp: %s, %s\n", __DATE__, __TIME__);
    Serial.println(s);
    char* token = strtok(s, delim); // get first token 
    // walk through tokens
    while (token != NULL) 
    {
        uint8_t m = str2month((const char*)token);
        if (m > 0) 
        {
            mt.month = m;
            //Serial.print(" month: "); Serial.println(mt.month);
            token = strtok(NULL, delim); // get next token
            mt.day = atoi(token);
            //Serial.print(" day: "); Serial.println(mt.day);
            token = strtok(NULL, delim); // get next token
            mt.year = atoi(token) - 1970;
           // Serial.print(" year: "); Serial.println(mt.year);
            token = strtok(NULL, delim); // get next token
            mt.hour = atoi(token);
            //Serial.print(" hour: "); Serial.println(mt.hour);
            token = strtok(NULL, delim); // get next token
            mt.minute = atoi(token);
            //Serial.print(" minute: "); Serial.println(mt.minute);
            token = strtok(NULL, delim); // get next token
            mt.second = atoi(token);
            //Serial.print(" second: "); Serial.println(mt.second);
        }
        token = strtok(NULL, delim);
    }
}

void setup() {
   // Wire.begin(); // стартуем I2C

#if debug
    Serial.begin(115200);
    Serial1.begin(115200);
    Serial.println("Setup started");
#endif
    Serial2.begin(9600); // старт UART для GPS модуля
    SettingsGPS.begin(Serial2);                   //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).
 
    
    
    SettingsGPS.baudrate(9600);                   //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
    SettingsGPS.system(GPS_GP, GPS_GL);           //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
    //SettingsGPS.composition(NMEA_ZDA);          //  Указываем что каждый пакет данных NMEA должен содержать только одну строку и этой строкой является идентификатор ZDA (информация о дате и времени).
    SettingsGPS.composition(NMEA_RMC);
    SettingsGPS.model(GPS_STATIC);                //  Указываем что модуль используется стационарно.
  //  SettingsGPS.updaterate(10);                   //  Указываем обновлять данные 10 раз в секунду. Функция gps.read() читает данные в 2 раза медленнее чем их выводит модуль.

    ParseBuildTimestamp(mtt);                     // получить эпоху Unix Время отсчитывается с 00:00:00 1 января 1970 г.
    tt = rtclock.makeTime(mtt) + 25;               // дополнительные секунды для компенсации задержки сборки и загрузки
    rtclock.setTime(tt);
    //Serial.print("tt: ");
    //Serial.println(tt);
    //sprintf(s, "RTC timestamp: %s %u %u, %s, %02u:%02u:%02u\n",
    //    months[mtt.month], mtt.day, mtt.year + 1970, weekdays[mtt.weekday], mtt.hour, mtt.minute, mtt.second);
    //Serial.print(s);


   // getGpsTime(); // получаем время GPS
   // writeRtc(); // записываем время в RTC
    Serial.println("Ethernet.begin started");
    // запускаем Ethernet шилд в режиме UDP:
    Ethernet.begin(mac, ip);
    Udp.begin(NTP_PORT);
    Serial.println("NTP started");


    //
    //  gps.read();                                   //
    //
    //      hour = gps.Hours;
    //      minute = gps.minutes;
    //      second = gps.seconds;
    //     // hundredths = timeStr.substring(7,10).toInt();
    //      day = gps.day;
    //      month = gps.month;
    //      year = gps.year; // передаются только десятки и единицы года
    //      writeRtc();      // записываем время в RTC
}

void loop() {
    processNTP(); // обрабатываем приходящие NTP запросы
}

String serStr; // строка для хранения пакетов от GPS приёмника

// Читает пакеты GPS приёмника из COM-порта и пытается найти в них время
// Если время найдено, возвращает True, иначе - False
void getGpsTime()
{
    bool timeFound = false;
    while (!timeFound)
    {
        while (Serial2.available() > 0)
        {
            char c = Serial2.read();
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
#if debug
    Serial.println("NMEA Packet = " + s);
#endif
    if (s.substring(0, 6) == "$GNRMC")
    {
        // Serial.println("NMEA Packet = " + s);
        String validFlag = s.substring(18, 19);
        // Ждём валидные данные (флаг "V" - данные не валидны, "A" - данные валидны):
        if (validFlag == "A")
        {
            String timeStr = s.substring(7, 17); // строка времени в формате ччммсс.сс
            hour = timeStr.substring(0, 2).toInt();
            minute = timeStr.substring(2, 4).toInt();
            second = timeStr.substring(4, 6).toInt();
            hundredths = timeStr.substring(7, 10).toInt();
            //Serial.println("timeStr = " + timeStr);

            // ищем индекс 4-ой запятой с конца, после которой идёт дата
            int commaIndex = 1;
            for (int i = 0; i < 6; i++)
            {
                commaIndex = s.lastIndexOf(",", commaIndex - 1);
            }
            String date = s.substring(commaIndex + 1, commaIndex + 7); // строка даты в формате ддммгг
           // String date = s.substring(commaIndex + 1, commaIndex + 7); // строка даты в формате ддммгг

            day = date.substring(0, 2).toInt();
            month = date.substring(2, 4).toInt();
            year = date.substring(4, 6).toInt(); // передаются только десятки и единицы года
#if debug
            printDate();
#endif
            return true;
        }
    }
    return false;
}

// Запоминает время в RTC
void writeRtc() 
{
    rtclock.setTime(mtt);

  //  getRtcDate();

 /*   sprintf(s, "RTC timestamp: %s %u %u, %s, %02u:%02u:%02u\n",
        months[mtt.month], mtt.day, mtt.year + 1970, weekdays[mtt.weekday], mtt.hour, mtt.minute, mtt.second);
    Serial.print(s);*/



#if debug
    Serial.print("Set date: ");
    printDate();
#endif
}

// Читает из RTC время и дату
void getRtcDate() 
{
    rtclock.getTime(mtt);

    day = mtt.day;
    month = mtt.month;
    year = mtt.year+1970;
    hour = mtt.hour;
    minute = mtt.minute;
    second = mtt.second;
    hundredths = 0;

#if debug
    Serial.print("Get date: ");
    printDate();
#endif
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

// Выводит отформатированноую дату
void printDate()
{
    char sz[32];
    sprintf(sz, "Date %02d.%02d.%04d %02d:%02d:%02d.%03d", day, month, year, hour, minute, second, hundredths);
    Serial.println(sz);
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
