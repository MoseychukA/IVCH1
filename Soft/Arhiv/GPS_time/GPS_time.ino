#include <iarduino_GPS_NMEA.h>                     //  ���������� ���������� ��� ����������� ����� ��������� NMEA ���������� �� UART.
#include <iarduino_GPS_ATGM336.h>                  //  ���������� ���������� ��� ��������� ���������� ������ GPS ������ ATGM336.
//
iarduino_GPS_NMEA    gps;                          //  ��������� ������ gps         ��� ������ � ��������� � �������� ���������� iarduino_GPS_NMEA.
iarduino_GPS_ATGM336 SettingsGPS;                  //  ��������� ������ SettingsGPS ��� ������ � ��������� � �������� ���������� iarduino_GPS_ATGM336.
                                                   //
char* wd[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };   //  ���������� ������ ����� ���������� �� ��� ������ ����� �� �������� ��� ������.
                                                   //
void setup() {                                      //
//   ���������� ������ ���������:                  //
    Serial.begin(9600);                           //  ���������� ������ � ���������� ����� UART ��� ������ ������ � ������� ����������������� ����� �� �������� 9600 ���/���.
    SettingsGPS.begin(Serial1);                   //  ���������� ������ � GPS ������� �� ��������� ���� UART. ������� ���� ��������� ������� �������� GPS ������ ATGM336 (������ ���������� ����, ����� ��������� �����������).
    gps.begin(Serial1);                           //  ���������� ����������� ����� NMEA ������ ������ ������������ ���� UART (������ ���������� ����, ����� ��������� �����������).
//   ����������� ������ ������:                    //
    SettingsGPS.baudrate(9600);                   //  ������������� �������� �������� ������ ������� � �������� ������ ���� Serial1 � 9600 ���/���.
    SettingsGPS.system(GPS_GP, GPS_GL);           //  ��������� ��� ������ ����� �������� �� ��������� ������������� ������ GPS (GPS_GP) � Glonass (GPS_GL).
    //SettingsGPS.composition(NMEA_ZDA);            //  ��������� ��� ������ ����� ������ NMEA ������ ��������� ������ ���� ������ � ���� ������� �������� ������������� ZDA (���������� � ���� � �������).
    SettingsGPS.composition(NMEA_RMC);
    SettingsGPS.model(GPS_STATIC);                //  ��������� ��� ������ ������������ �����������.
    SettingsGPS.updaterate(10);                   //  ��������� ��������� ������ 10 ��� � �������. ������� gps.read() ������ ������ � 2 ���� ��������� ��� �� ������� ������.
   // SettingsGPS.system(GPS_FACTORY_SET);
}                                                  //
                                                   //
void loop() {                                       //
//   ������ ������:                                //
    gps.read();                                   //
//   ������� �����:                                //
    Serial.print(gps.Hours); Serial.print(":"); //  ������� ���.
    Serial.print(gps.minutes); Serial.print(":"); //  ������� ������.
    Serial.print(gps.seconds); Serial.print(" "); //  ������� �������.
//   ������� ����:                                 //
    Serial.print(gps.day); Serial.print("."); //  ������� ���� ������.
    Serial.print(gps.month); Serial.print("."); //  ������� �����.
    Serial.print(gps.year); Serial.print("year.");//  ������� ���.
//   ������� ���� ������:                          //
    Serial.print(" (");                           //
    Serial.print(wd[gps.weekday]);                //  ������� ���� ������.
    Serial.print("), ");                          //
//   ������� ���������� ������ � ������ ����� Unix //
    Serial.print("UnixTime: ");                   //
    Serial.print(gps.Unix);                       //  ������� ����� UnixTime.
    Serial.print("s.");                           //
//   ������� ���������� � ������� ������:          //
    if (gps.errTim) {                             //
        Serial.print(" The time is unreliable.");     //  ������� ���������� � ������������� �������.
    }                                             //
    if (gps.errDat) {                             //
        Serial.print(" The date is unreliable.");      //  ������� ���������� � ������������� ����.
    }                                             //
//   ��������� ������:                             //
    Serial.print("\r\n");                         //
}