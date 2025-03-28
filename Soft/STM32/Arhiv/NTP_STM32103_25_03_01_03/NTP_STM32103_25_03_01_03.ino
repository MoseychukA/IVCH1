#include <SPI.h>
#include <Ethernet_STM.h>
#include <EthernetUdp.h>
#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
#include <RTClock.h>
#include <EEPROM.h>
#include <libmaple/iwdg.h>
#include <Wire_slave.h>
#include <Arduino.h>

#define USE_BUFFERED_EXAMPLE 1

i2c_msg msg;
uint8 buffer[255];
char* const bufferAsChar = (char*)buffer; // ready type casted alias

volatile bool newMessage = false;

void funcrx(i2c_msg* msg __attribute__((unused))) 
{
    // Received length will be in msg->length
    newMessage = true;
}

#if USE_BUFFERED_EXAMPLE == 1
/* We ARE using a buffer to transmit the data out.
* Make sure you fill the buffer with the data AND you set the length correctly
*/
void functx(i2c_msg* msg) {
    // Cheeky. We are using the received byte of the data which is currently in
    // byte 0 to echo it back to the master device
    //msg->data[0] = 0x01;    // We are re-using the rx buffer here to echo the request back
    msg->data[1] = 0x02;
    msg->data[2] = 0x03;
    msg->data[3] = 0x04;
    msg->data[4] = 0x05;
    msg->length = 5;
}

#else

/* We are NOT using the buffered data transmission
* We will get this callback for each outgoing packet. Make sure to call i2c_write
* Strickly speaking, we should be sending a NACk on the last byte we want to send
* but for this test example I am going to assume the master will NACK it when it
* wants to stop.
*/
void functx(i2c_msg* msg) {
    i2c_write(I2C1, msg->data[0]);
}

#endif


#define debug true // для вывода отладочных сообщений

#define STARTOFTIME 2208988800UL

#ifndef UTIL_H
#define UTIL_H

#define htons(x) ( ((x)<< 8 & 0xFF00) | \
                   ((x)>> 8 & 0x00FF) )
#define ntohs(x) htons(x)

#define htonl(x) ( ((x)<<24 & 0xFF000000UL) | \
                   ((x)<< 8 & 0x00FF0000UL) | \
                   ((x)>> 8 & 0x0000FF00UL) | \
                   ((x)>>24 & 0x000000FFUL) )
#define ntohl(x) htonl(x)
#endif

/* Shifts usecs in unixToNtpTime */
#ifndef USECSHIFT
#define USECSHIFT (1LL << 32) * 1.0e-6
#endif


iarduino_GPS_ATGM336 SettingsGPS;                  //  Объявляем объект SettingsGPS для работы с функциями и методами библиотеки iarduino_GPS_ATGM336.

EthernetUDP Udp;

USBSerial SerialUSB;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
#if defined(WIZ550io_WITH_MACADDRESS) // Use assigned MAC address of WIZ550io
;
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#endif  

#define NTP_PORT 123 // стандартный порт, не менять
IPAddress timeServer(213, 202, 247, 29); // time-a.timefreq.bldrdoc.gov NTP server

/*Default ip*/
uint8_t myip[] = { 192,168,1,177 };

EthernetServer server(80);


static const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

int year;
byte month, day, hour, minute, second, hundredths;
uint32_t timestamp, tempval;
 
RTClock rtclock(RTCSEL_LSE); // initialise

time_t tt;
tm_t mtt;
uint8_t dateread[11];
bool dispflag = true;
bool newTimeGPS = false;


/* Test diode*/
const int ledPin = 45;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long previousMillisNTP = 0;
const long interval = 1000;               // interval at which to blink (milliseconds)
const long intervalNTP = 30000;           // interval at which to blink (milliseconds)
int count = 0;

#ifndef PACKETSIZE
#define PACKETSIZE 48
#endif

static char lat_s = '0', lon_s = '0', qual = '0', stateGPS = '0', command = '0';
String lat, lon, date_GPS, sats, time_GPS;
boolean renew = false;
uint32_t unixTime_last_sync = 0, Time_last_sync = 0;

/* SNTP Packet array */
char serverPacket[PACKETSIZE] = { 0 };
char echoPacket[10] = { 0xF };

/* port to listen to sntp messages */
static char srcPort = 123;

uint32_t micros_recv = 0;
uint32_t timeStamp;
extern uint32_t micros_offset, uptime_sec_count;
extern time_t t;
//!!extern ModbusIP mb;
//!!extern int mb_begin_offset;
uint32_t micros_transmit = 0;



void setup() 
{
 
    SerialUSB.begin(115200);
    while (!SerialUSB && millis() < 2000);
    //while (!SerialUSB) {
    //    ; // wait for serial port to connect. Needed for native USB port only
    //}
     pinMode(ledPin, OUTPUT);

    Serial1.begin(9600);
    Serial2.begin(9600);                          // старт UART для GPS модуля
    Serial3.begin(9600);                          // RS485
    SettingsGPS.begin(Serial2);                   //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).
  
    SettingsGPS.baudrate(9600);                   //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
    SettingsGPS.system(GPS_GP, GPS_GL);           //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
    SettingsGPS.composition(NMEA_RMC);
    SettingsGPS.model(GPS_STATIC);                //  Указываем что модуль используется стационарно.
    SettingsGPS.updaterate(10);                   //  Указываем обновлять данные 10 раз в секунду. Функция gps.read() читает данные в 2 раза медленнее чем их выводит модуль.

    iwdg_init(IWDG_PRE_256, 4000); // init watchdog timer
 
    Wire.begin(); // присоединение к шине i2c (адрес необязателен для мастера)
   
 
    uint16_t chksum = (uint16_t)EEPROM.read(4) + (uint16_t)EEPROM.read(5) * 256;

    /* if chksum is OK use ip from "eeprom" */

    if ((chksum == (uint8_t)EEPROM.read(0) + (uint8_t)EEPROM.read(1) +
        (uint8_t)EEPROM.read(2) + (uint8_t)EEPROM.read(3))
        && chksum > 4 && chksum < 1000)
    {
        uint8_t ip[] = { (uint8_t)EEPROM.read(0), (uint8_t)EEPROM.read(1),
                         (uint8_t)EEPROM.read(2), (uint8_t)EEPROM.read(3) };
        memcpy(myip, ip, 4); 
    }

    SerialUSB.println("We are waiting for the time GPS!");

    getGpsTime(); // получаем время GPS
   // writeRtc(); // записываем время в RTC
    SerialUSB.println("Ethernet.begin started");



    // запускаем Ethernet шилд в режиме UDP:
    Ethernet.begin(mac, myip);
    Udp.begin(NTP_PORT);
    SerialUSB.println("NTP started");

    server.begin();
    Serial.print("Server is at ");
    Serial.println(Ethernet.localIP());

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    SerialUSB.print("Version: ");
    SerialUSB.println(ver_soft);
}

void loop() 
{
    processNTP();                                           // обрабатываем приходящие NTP запросы
    check_for_serial_commands((HardwareSerial*)&SerialUSB); //
    processGNRMC(&Serial2);                                 //
    iwdg_feed(); //feed watchdog


   // listen for incoming clients
    EthernetClient client = server.available();
    if (client) {
        Serial.println("new client");
        // an http request ends with a blank line
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                // if you've gotten to the end of the line (received a newline
                // character) and the line is blank, the http request has ended,
                // so you can send a reply
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println("Connection: close");  // the connection will be closed after completion of the response
                    client.println("Refresh: 5");  // refresh the page automatically every 5 sec
                    client.println();
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html>");
                    // output the value of each analog input pin
                    for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
                        int sensorReading = analogRead(analogChannel);
                        client.print("analog input ");
                        client.print(analogChannel);
                        client.print(" is ");
                        client.print(sensorReading);
                        client.println("<br />");
                    }
                    client.println("</html>");
                    break;
                }
                if (c == '\n') {
                    // you're starting a new line
                    currentLineIsBlank = true;
                }
                else if (c != '\r') {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                }
            }
        }
        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        Serial.println("client disonnected");
    }



    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        getRtcDate();

        if (newTimeGPS)
        {
            // SerialUSB.println("TimeGPS Ok!");
        }
        else
        {
           // SerialUSB.println("TimeGPS false!");

            if (currentMillis - previousMillisNTP >= intervalNTP)
            {
                // save the last time you blinked the LED
                previousMillisNTP = currentMillis;

                sendNTPpacket(timeServer); // send an NTP packet to a time server
                delay(1000);
                if (Udp.parsePacket())
                {
                    SerialUSB.println("**Udp.parsePacket");
                    // We've received a packet, read the data from it
                    Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer

                    //the timestamp starts at byte 40 of the received packet and is four bytes,
                    // or two words, long. First, esxtract the two words:

                    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
                    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
                    // combine the four bytes (two words) into a long integer
                    // this is NTP time (seconds since Jan 1 1900):
                    unsigned long secsSince1900 = highWord << 16 | lowWord;
                    SerialUSB.print("Seconds since Jan 1 1900 = ");
                    SerialUSB.println(secsSince1900);

                    // now convert NTP time into everyday time:
                    SerialUSB.print("Unix time = ");
                    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
                    const unsigned long seventyYears = 2208988800UL;
                    // subtract seventy years:
                    unsigned long epoch = secsSince1900 - seventyYears;
                    // print Unix time:
                    SerialUSB.println(epoch);

                    // print the hour, minute and second:
                    SerialUSB.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
                    SerialUSB.print((epoch % 86400L) / 3600);  // print the hour (86400 equals secs per day)
                    SerialUSB.print(':');
                    if (((epoch % 3600) / 60) < 10) {
                        // In the first 10 minutes of each hour, we'll want a leading '0'
                        SerialUSB.print('0');
                    }
                    SerialUSB.print((epoch % 3600) / 60); // print the minute (3600 equals secs per minute)
                    SerialUSB.print(':');
                    if ((epoch % 60) < 10) {
                        // In the first 10 seconds of each minute, we'll want a leading '0'
                        SerialUSB.print('0');
                    }
                    SerialUSB.println(epoch % 60); // print the second


                      // print the hour, minute and second:
                  //  Serial.print(F("The UTC/GMT time is "));       // UTC is the time at Greenwich Meridian (GMT)

                    rtclock.setTime(epoch);

                }
            }
            // wait ten seconds before asking for the time again
        }

        // if the LED is off turn it on and vice-versa:
        if (ledState == LOW) 
        {
            ledState = HIGH;
        }
        else 
        {
            ledState = LOW;
        }

        // set the LED with the ledState of the variable:
        digitalWrite(ledPin, ledState);
        //Serial.print("loop dhtp: ");
        //Serial.println(count);
        count++;
        if (count == 999)count = 0;
    }

}

String serStr; // строка для хранения пакетов от GPS приёмника

// Читает пакеты GPS приёмника из COM-порта и пытается найти в них время
// Если время найдено, возвращает True, иначе - False
void getGpsTime()
{
    bool timeFound = false;
    while (!timeFound && millis() < 20000)
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
   // SerialUSB.println("NMEA Packet = " + s);
#endif
    if (s.substring(0, 6) == "$GNRMC")
    {
        String validFlag = s.substring(18, 19);
        // Ждём валидные данные (флаг "V" - данные не валидны, "A" - данные валидны):
        if (validFlag == "A")
        {
            String timeStr = s.substring(7, 17); // строка времени в формате ччммсс.сс
            hour = timeStr.substring(0, 2).toInt();
            minute = timeStr.substring(2, 4).toInt();
            second = timeStr.substring(4, 6).toInt();
            hundredths = timeStr.substring(7, 10).toInt();
            //SerialUSB.println("timeStr = " + timeStr);

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

 #if debug
    SerialUSB.print("Set date: ");
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
    SerialUSB.print("Get date: ");
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
        SerialUSB.println();
        SerialUSB.print("Received UDP packet size ");
        SerialUSB.println(packetSize);
        SerialUSB.print("From ");

        for (int i = 0; i < 4; i++)
        {
            SerialUSB.print(remote[i], DEC);
            if (i < 3) { SerialUSB.print("."); }
        }
        SerialUSB.print(", port ");
        SerialUSB.print(portNum);

        byte LIVNMODE = packetBuffer[0];
        SerialUSB.print("  LI, Vers, Mode :");
        SerialUSB.print(packetBuffer[0], HEX);

        byte STRATUM = packetBuffer[1];
        SerialUSB.print("  Stratum :");
        SerialUSB.print(packetBuffer[1], HEX);

        byte POLLING = packetBuffer[2];
        SerialUSB.print("  Polling :");
        SerialUSB.print(packetBuffer[2], HEX);

        byte PRECISION = packetBuffer[3];
        SerialUSB.print("  Precision :");
        SerialUSB.println(packetBuffer[3], HEX);

        for (int z = 0; z < NTP_PACKET_SIZE; z++) 
        {
            SerialUSB.print(packetBuffer[z], HEX);
            if (((z + 1) % 4) == 0) 
            {
                SerialUSB.println();
            }
        }
        SerialUSB.println();
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
        SerialUSB.println("Timestamp = " + (String)timestamp);
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
    SerialUSB.println(sz);
}

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
        SerialUSB.println("Wrong IP in change ip command \n");
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

        SerialUSB.print("\nNew IP is: " + String(ipAddress[0]) + '.' + String(ipAddress[1]) + '.' + String(ipAddress[2]) + '.' + String(ipAddress[3]) + '\n');
        SerialUSB.print("Rebooting \n");
        for (;;);
    }
    else
    {
        SerialUSB.println("Invalid IP Checksum \n");
        SerialUSB.println("Correct format example: 192,168,0,121,481, \n");
        SerialUSB.println("192+168+0+121 = 481");
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

        case '3': if ('R' == b) stateGPS = '4'; else stateGPS = '0'; break;

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

        default: stateGPS = '0'; break;
        }

   /* if found flag A, we sync every 300 seconds todo : setup a stack with last 5 values from gps */
        if (renew && lat[0] == 'A' && (rtclock.getTime() - Time_last_sync) > 5)
        {
            int hours = 0, minutes = 0, seconds = 0, days = 0, months = 0, years = 0;
            hours   += (time_GPS[0] - '0') * 10;
            hours   += time_GPS[1]  - '0';
            minutes += (time_GPS[2] - '0') * 10;
            minutes += time_GPS[3]  - '0';
            seconds += (time_GPS[4] - '0') * 10;
            seconds += time_GPS[5]  - '0';

            days   += (date_GPS[0] - '0') * 10;
            days   += date_GPS[1]  - '0';
            months += (date_GPS[2] - '0') * 10;
            months += date_GPS[3]  - '0';
            years  += (date_GPS[4] - '0') * 10;
            years  += date_GPS[5]  - '0';

            /* set internal time with time received from gps */
            set_Internal_time_GPS(hours, minutes, seconds, days, months, years);

            SerialUSB.print("\r\ntime_GPS = " + time_GPS + " date_GPS = " + date_GPS + "\r\n");
            tm_t gps_time = { (uint8_t)(years+2000-1970), (uint8_t)months, (uint8_t)days, 0, 0, (uint8_t)hours, (uint8_t)minutes, (uint8_t)seconds };
            rtclock.setTime(gps_time);
            renew = false;
            unixTime_last_sync = htonl(rtclock.getTime() + STARTOFTIME);
            Time_last_sync = rtclock.getTime();
            //micros_offset = (micros()) % 1000000;

            newTimeGPS = true;
            /* установить маркер A в 0 регистре modbus, это означает, что время готово к считыванию */
          //  mb.Hreg(mb_begin_offset + 0, 'A');
        }
        /* set marker V in 0 register of modbus, - time is NOT valid */
        else if ((rtclock.getTime() - Time_last_sync) > 5)
        {
           // mb.Hreg(mb_begin_offset + 0, 'V');
            newTimeGPS = false;
        }
    }
}

void set_Internal_time_GPS(uint8_t hr, uint8_t min, uint8_t sec, uint8_t day, uint8_t mnth, int yr)
{
    tm_t gps_time = { (uint8_t)(yr - 1970), mnth, day, 0, 0, hr, min, sec };
    rtclock.setTime(gps_time);
}


// отправить NTP-запрос на сервер времени по указанному адресу
unsigned long sendNTPpacket(IPAddress& address)
{
    // устанавливаем все байты в буфере на 0
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    // Инициализируем значения, необходимые для формирования запроса NTP
    // (см. URL выше для получения подробной информации о пакетах)
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;            // Stratum, or type of clock
    packetBuffer[2] = 6;            // Polling Interval
    packetBuffer[3] = 0xEC;         // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    // всем полям NTP присвоены значения, теперь
   // можно отправить пакет с запросом временной метки:	   
    Udp.beginPacket(address, 123);   //NTP requests are to port 123
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
}



