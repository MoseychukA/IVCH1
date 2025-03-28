#include <SPI.h>
#include <Ethernet_STM.h>
#include <EthernetUdp.h>
#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.

#define debug true // для вывода отладочных сообщений

iarduino_GPS_ATGM336 SettingsGPS;                  //  Объявляем объект SettingsGPS для работы с функциями и методами библиотеки iarduino_GPS_ATGM336.

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
#if defined(WIZ550io_WITH_MACADDRESS) // Use assigned MAC address of WIZ550io
;
#else
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
#endif  

IPAddress ip(192, 168, 1, 177); // задайте свой IP
#define NTP_PORT 123            // стандартный порт, не менять

IPAddress timeServer(213, 202, 247, 29); // time-b.timefreq.bldrdoc.gov NTP server

const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

int year;
byte month, day, hour, minute, second, hundredths;
unsigned long date, time, age;
uint32_t timestamp, tempval;

const int ledPin = PC13;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 10000;           // interval at which to blink (milliseconds)





void setup() 
{
  //Initialize serial and wait for port to open:
  Serial.begin(115200);
  Serial1.begin(115200);
  Serial2.begin(115200);
  Serial3.begin(115200);
  SettingsGPS.begin(Serial2);                   //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).
  SettingsGPS.baudrate(9600);                   //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
  SettingsGPS.system(GPS_GP, GPS_GL);           //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
  //SettingsGPS.composition(NMEA_ZDA);          //  Указываем что каждый пакет данных NMEA должен содержать только одну строку и этой строкой является идентификатор ZDA (информация о дате и времени).
  SettingsGPS.composition(NMEA_RMC);
  SettingsGPS.model(GPS_STATIC);                //  Указываем что модуль используется стационарно.
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // prints title with ending line break
// Serial.println("ASCII Table ~ Character Map");
delay(1000);
Serial.println("Start");

  // start Ethernet and UDP
#if defined(WIZ550io_WITH_MACADDRESS)
  if (Ethernet.begin() == 0) {
#else
  if (Ethernet.begin(mac) == 0) 
  {
#endif  
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  Udp.begin(NTP_PORT);

  Serial.println("Setup End");
}

void loop()
{

    processNTP(); // обрабатываем приходящие NTP запросы

    unsigned long currentMillis = millis();

    //if (currentMillis - previousMillis >= interval)
    //{
    //    // save the last time you blinked the LED
    //    previousMillis = currentMillis;

    //    // if the LED is off turn it on and vice-versa:
    //    if (ledState == LOW) 
    //    {
    //        ledState = HIGH;
    //    }
    //    else 
    //    {
    //        ledState = LOW;
    //    }

    //    // set the LED with the ledState of the variable:
    //    digitalWrite(ledPin, ledState);


    //    sendNTPpacket(timeServer); //отправить NTP-пакет на сервер времени
    //      // подождите, чтобы увидеть, доступен ли ответ
    //    delay(1000);
    //    if (Udp.parsePacket())
    //    {
    //        // We've received a packet, read the data from it
    //        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer

    //        unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    //        unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);

    //        unsigned long secsSince1900 = highWord << 16 | lowWord;
    //        Serial.print("Seconds since Jan 1 1900 = ");
    //        Serial.println(secsSince1900);

    //        // now convert NTP time into everyday time:
    //        Serial.print("Unix time = ");
    //        // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    //        const unsigned long seventyYears = 2208988800UL;
    //        // subtract seventy years:
    //        unsigned long epoch = secsSince1900 - seventyYears;
    //        // print Unix time:
    //        Serial.println(epoch);


    //        // print the hour, minute and second:
    //        Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    //        Serial.print((epoch % 86400L) / 3600); // print the hour (86400 equals secs per day)
    //        Serial.print(':');
    //        if (((epoch % 3600) / 60) < 10)
    //        {
    //            // In the first 10 minutes of each hour, we'll want a leading '0'
    //            Serial.print('0');
    //        }
    //        Serial.print((epoch % 3600) / 60); // print the minute (3600 equals secs per minute)
    //        Serial.print(':');
    //        if ((epoch % 60) < 10)
    //        {
    //            // In the first 10 seconds of each minute, we'll want a leading '0'
    //            Serial.print('0');
    //        }
    //        Serial.println(epoch % 60); // print the second
    //    }

    //}
}

// send an NTP request to the time server at the given address 
unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE); 
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49; 
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:        
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket(); 
}


String serStr; // строка для хранения пакетов от GPS приёмника

// Читает пакеты GPS приёмника из COM-порта и пытается найти в них время
// Если время найдено, возвращает True, иначе - False
void getGpsTime()
{
    bool timeFound = false;
    while (!timeFound)
    {
        while (Serial1.available() > 0)
        {
            char c = Serial1.read();
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
    byte arr[] = { 0x00, dec2hex(second), dec2hex(minute), dec2hex(hour), 0x01, dec2hex(day), dec2hex(month), dec2hex(year) };
//    Wire.beginTransmission(RTC_ADDR);
//    Wire.write(arr, 8);
//    Wire.endTransmission();
//#if debug
//    Serial.print("Set date: ");
//    printDate();
//#endif
}

// Преобразует число из dec представления в hex представление
byte dec2hex(byte b) {
    String bs = (String)b;
    byte res;
    if (bs.length() == 2) {
        res = String(bs.charAt(0)).toInt() * 16 + String(bs.charAt(1)).toInt();
    }
    else {
        res = String(bs.charAt(0)).toInt();
    }
#if debug
    Serial.println("dec " + (String)b + " = hex " + (String)res);
#endif  
    return res;
}

// Читает из RTC время и дату
void getRtcDate() 
{
//    Wire.beginTransmission(RTC_ADDR);
//    Wire.write(byte(0));
//    Wire.endTransmission();
//
//    Wire.beginTransmission(RTC_ADDR);
//    Wire.requestFrom(RTC_ADDR, 7);
//    byte t[7];
//    int i = 0;
//    while (Wire.available())
//    {
//        t[i] = Wire.read();
//        i++;
//    }
//    Wire.endTransmission();
//    second = t[0];
//    minute = t[1];
//    hour = t[2];
//    day = t[4];
//    month = t[5];
//    year = t[6];
//#if debug
//    Serial.print("Get date: ");
//    printDate();
//#endif
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

        for (int z = 0; z < NTP_PACKET_SIZE; z++) {
            Serial.print(packetBuffer[z], HEX);
            if (((z + 1) % 4) == 0) {
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
    sprintf(sz, "Date %02d.%02d.%04d %02d:%02d:%02d.%03d", day, month, year + 2000, hour, minute, second, hundredths);
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
