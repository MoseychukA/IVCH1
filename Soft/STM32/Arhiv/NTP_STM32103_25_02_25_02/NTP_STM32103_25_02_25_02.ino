#include <SPI.h>
#include <Ethernet_STM.h>
#include <EthernetUdp.h>
#include <iarduino_GPS_ATGM336.h>                    //  ���������� ���������� ��� ��������� ���������� ������ GPS ������ ATGM336.
#include <RTClock.h>

#define debug true // ��� ������ ���������� ���������

iarduino_GPS_ATGM336 SettingsGPS;                  //  ��������� ������ SettingsGPS ��� ������ � ��������� � �������� ���������� iarduino_GPS_ATGM336.

EthernetUDP Udp;

USBSerial SerialUSB;

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
#if defined(WIZ550io_WITH_MACADDRESS) // Use assigned MAC address of WIZ550io
;
#else
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
#endif  

IPAddress ip(192, 168, 1, 177); // ������� ���� IP
#define NTP_PORT 123 // ����������� ����, �� ������

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
    SerialUSB.println(s);
    char* token = strtok(s, delim); // get first token 
    // walk through tokens
    while (token != NULL) 
    {
        uint8_t m = str2month((const char*)token);
        if (m > 0) 
        {
            mt.month = m;
            //SerialUSB.print(" month: "); SerialUSB.println(mt.month);
            token = strtok(NULL, delim); // get next token
            mt.day = atoi(token);
            //SerialUSB.print(" day: "); SerialUSB.println(mt.day);
            token = strtok(NULL, delim); // get next token
            mt.year = atoi(token) - 1970;
           // SerialUSB.print(" year: "); SerialUSB.println(mt.year);
            token = strtok(NULL, delim); // get next token
            mt.hour = atoi(token);
            //SerialUSB.print(" hour: "); SerialUSB.println(mt.hour);
            token = strtok(NULL, delim); // get next token
            mt.minute = atoi(token);
            //SerialUSB.print(" minute: "); SerialUSB.println(mt.minute);
            token = strtok(NULL, delim); // get next token
            mt.second = atoi(token);
            //SerialUSB.print(" second: "); SerialUSB.println(mt.second);
        }
        token = strtok(NULL, delim);
    }
}



const int ledPin = 45;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)

int count = 0;


void setup() 
{
 
    SerialUSB.begin(115200);
    //delay(2500);
    while (!SerialUSB && millis() < 2000);
    //while (!SerialUSB) {
    //    ; // wait for serial port to connect. Needed for native USB port only
    //}
    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    SerialUSB.print("Version: ");
    SerialUSB.println(ver_soft);

    pinMode(ledPin, OUTPUT);

    Serial1.begin(115200);
    Serial2.begin(9600); // ����� UART ��� GPS ������
    SettingsGPS.begin(Serial2);                   //  ���������� ������ � GPS ������� �� ��������� ���� UART. ������� ���� ��������� ������� �������� GPS ������ ATGM336 (������ ���������� ����, ����� ��������� �����������).
  
    SettingsGPS.baudrate(9600);                   //  ������������� �������� �������� ������ ������� � �������� ������ ���� Serial1 � 9600 ���/���.
    SettingsGPS.system(GPS_GP, GPS_GL);           //  ��������� ��� ������ ����� �������� �� ��������� ������������� ������ GPS (GPS_GP) � Glonass (GPS_GL).
    //SettingsGPS.composition(NMEA_ZDA);          //  ��������� ��� ������ ����� ������ NMEA ������ ��������� ������ ���� ������ � ���� ������� �������� ������������� ZDA (���������� � ���� � �������).
    SettingsGPS.composition(NMEA_RMC);
    SettingsGPS.model(GPS_STATIC);                //  ��������� ��� ������ ������������ �����������.
  //  SettingsGPS.updaterate(10);                   //  ��������� ��������� ������ 10 ��� � �������. ������� gps.read() ������ ������ � 2 ���� ��������� ��� �� ������� ������.

    ParseBuildTimestamp(mtt);                     // �������� ����� Unix ����� ������������� � 00:00:00 1 ������ 1970 �.
    tt = rtclock.makeTime(mtt) + 25;               // �������������� ������� ��� ����������� �������� ������ � ��������
    rtclock.setTime(tt);
    //SerialUSB.print("tt: ");
    //SerialUSB.println(tt);
    //sprintf(s, "RTC timestamp: %s %u %u, %s, %02u:%02u:%02u\n",
    //    months[mtt.month], mtt.day, mtt.year + 1970, weekdays[mtt.weekday], mtt.hour, mtt.minute, mtt.second);
    //SerialUSB.print(s);


    getGpsTime(); // �������� ����� GPS
   // writeRtc(); // ���������� ����� � RTC
    SerialUSB.println("Ethernet.begin started");
    // ��������� Ethernet ���� � ������ UDP:
    Ethernet.begin(mac, ip);
    Udp.begin(NTP_PORT);
    //.println("NTP started");

    //String ver_soft = __FILE__;
    //int val_srt = ver_soft.lastIndexOf('\\');
    //ver_soft.remove(0, val_srt + 1);
    //val_srt = ver_soft.lastIndexOf('.');
    //ver_soft.remove(val_srt);
    //SerialUSB.print("Version: ");
    //SerialUSB.println(ver_soft);
    //
    //  gps.read();                                   //
    //
    //      hour = gps.Hours;
    //      minute = gps.minutes;
    //      second = gps.seconds;
    //     // hundredths = timeStr.substring(7,10).toInt();
    //      day = gps.day;
    //      month = gps.month;
    //      year = gps.year; // ���������� ������ ������� � ������� ����
    //      writeRtc();      // ���������� ����� � RTC
}

void loop() {
    processNTP(); // ������������ ���������� NTP �������

   // processGNRMC(&Serial2);

  //  Serial.println("loop dhtp");
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

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

String serStr; // ������ ��� �������� ������� �� GPS ��������

// ������ ������ GPS �������� �� COM-����� � �������� ����� � ��� �����
// ���� ����� �������, ���������� True, ����� - False
void getGpsTime()
{
    bool timeFound = false;
    while (!timeFound)
    {
     /*   if (Serial2.available() > 0)
        {*/
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
        //}
        //else
        //{
        //    timeFound = true;
        //   // break;
        //}
    }
}

// ���������� ����� �� NMEA ������ 
// � ���������� True � ������ ������ � False � �������� ������
bool decodeTime(String s)
{
#if debug
    SerialUSB.println("NMEA Packet = " + s);
#endif
    if (s.substring(0, 6) == "$GNRMC")
    {
        String validFlag = s.substring(18, 19);
        // ��� �������� ������ (���� "V" - ������ �� �������, "A" - ������ �������):
        if (validFlag == "A")
        {
            String timeStr = s.substring(7, 17); // ������ ������� � ������� ������.��
            hour = timeStr.substring(0, 2).toInt();
            minute = timeStr.substring(2, 4).toInt();
            second = timeStr.substring(4, 6).toInt();
            hundredths = timeStr.substring(7, 10).toInt();
            //SerialUSB.println("timeStr = " + timeStr);

            // ���� ������ 4-�� ������� � �����, ����� ������� ��� ����
            int commaIndex = 1;
            for (int i = 0; i < 6; i++)
            {
                commaIndex = s.lastIndexOf(",", commaIndex - 1);
            }
            String date = s.substring(commaIndex + 1, commaIndex + 7); // ������ ���� � ������� ������
           // String date = s.substring(commaIndex + 1, commaIndex + 7); // ������ ���� � ������� ������

            day = date.substring(0, 2).toInt();
            month = date.substring(2, 4).toInt();
            year = date.substring(4, 6).toInt(); // ���������� ������ ������� � ������� ����
#if debug
            printDate();
#endif
            return true;
        }
    }
    return false;
}

// ���������� ����� � RTC
void writeRtc() 
{
    rtclock.setTime(mtt);

  //  getRtcDate();

 /*   sprintf(s, "RTC timestamp: %s %u %u, %s, %02u:%02u:%02u\n",
        months[mtt.month], mtt.day, mtt.year + 1970, weekdays[mtt.weekday], mtt.hour, mtt.minute, mtt.second);
    SerialUSB.print(s);*/



#if debug
    SerialUSB.print("Set date: ");
    printDate();
#endif
}

// ������ �� RTC ����� � ����
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

// ������������ ������� � NTP �������
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
        SerialUSB.println("Timestamp = " + (String)timestamp);
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

// ������� ������������������ ����
void printDate()
{
    char sz[32];
    sprintf(sz, "Date %02d.%02d.%04d %02d:%02d:%02d.%03d", day, month, year, hour, minute, second, hundredths);
    SerialUSB.println(sz);
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



void PickGPSFix()
{
    bool isValidSentence = false;
    int ndx;
    int c = -1;

    /*
     * Check SW/HW UARTs, USB and BT for data
     * WARNING! Make use only one input source at a time.
     */
    while (true) 
    {
        if (Serial2.available() > 0) 
        {
            c = Serial2.read();
        }
        else 
        {
            /* return back if no input data */
            break;
        }

        if (c == -1)
        {
            /* retry */
            continue;
        }

        //if (isPrintable(c) || c == '\r' || c == '\n') 
        //{
        //    GNSSbuf[GNSS_cnt] = c;
        //}
        //else 
        //{
        //    /* ignore */
        //    continue;
        //}

      //  isValidSentence = gnss.encode(GNSSbuf[GNSS_cnt]);
    



      /*  if (GNSSbuf[GNSS_cnt] == '\n' || GNSS_cnt == sizeof(GNSSbuf) - 1) 
        {
            GNSS_cnt = 0;
        }
        else
        {
            GNSS_cnt++;
            yield();
        }*/
    }
}
