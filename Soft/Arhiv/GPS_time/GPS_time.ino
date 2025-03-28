#include <iarduino_GPS_NMEA.h>                     //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.
#include <iarduino_GPS_ATGM336.h>                  //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
//
iarduino_GPS_NMEA    gps;                          //  Объявляем объект gps         для работы с функциями и методами библиотеки iarduino_GPS_NMEA.
iarduino_GPS_ATGM336 SettingsGPS;                  //  Объявляем объект SettingsGPS для работы с функциями и методами библиотеки iarduino_GPS_ATGM336.
                                                   //
char* wd[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };   //  Определяем массив строк содержащих по две первых буквы из названий дня недели.
                                                   //
void setup() {                                      //
//   Инициируем работу библиотек:                  //
    Serial.begin(9600);                           //  Инициируем работу с аппаратной шиной UART для вывода данных в монитор последовательного порта на скорости 9600 бит/сек.
    SettingsGPS.begin(Serial1);                   //  Инициируем работу с GPS модулем по указанной шине UART. Функция сама определит текущую скорость GPS модуля ATGM336 (вместо аппаратной шины, можно указывать программную).
    gps.begin(Serial1);                           //  Инициируем расшифровку строк NMEA указав объект используемой шины UART (вместо аппаратной шины, можно указывать программную).
//   Настраиваем работу модуля:                    //
    SettingsGPS.baudrate(9600);                   //  Устанавливаем скорость передачи данных модулем и скорость работы шины Serial1 в 9600 бит/сек.
    SettingsGPS.system(GPS_GP, GPS_GL);           //  Указываем что данные нужно получать от спутников навигационных систем GPS (GPS_GP) и Glonass (GPS_GL).
    //SettingsGPS.composition(NMEA_ZDA);            //  Указываем что каждый пакет данных NMEA должен содержать только одну строку и этой строкой является идентификатор ZDA (информация о дате и времени).
    SettingsGPS.composition(NMEA_RMC);
    SettingsGPS.model(GPS_STATIC);                //  Указываем что модуль используется стационарно.
    SettingsGPS.updaterate(10);                   //  Указываем обновлять данные 10 раз в секунду. Функция gps.read() читает данные в 2 раза медленнее чем их выводит модуль.
   // SettingsGPS.system(GPS_FACTORY_SET);
}                                                  //
                                                   //
void loop() {                                       //
//   Читаем данные:                                //
    gps.read();                                   //
//   Выводим время:                                //
    Serial.print(gps.Hours); Serial.print(":"); //  Выводим час.
    Serial.print(gps.minutes); Serial.print(":"); //  Выводим минуты.
    Serial.print(gps.seconds); Serial.print(" "); //  Выводим секунды.
//   Выводим дату:                                 //
    Serial.print(gps.day); Serial.print("."); //  Выводим день месяца.
    Serial.print(gps.month); Serial.print("."); //  Выводим месяц.
    Serial.print(gps.year); Serial.print("year.");//  Выводим год.
//   Выводим день недели:                          //
    Serial.print(" (");                           //
    Serial.print(wd[gps.weekday]);                //  Выводим день недели.
    Serial.print("), ");                          //
//   Выводим количество секунд с начала эпохи Unix //
    Serial.print("UnixTime: ");                   //
    Serial.print(gps.Unix);                       //  Выводим время UnixTime.
    Serial.print("s.");                           //
//   Выводим информацию о наличии ошибок:          //
    if (gps.errTim) {                             //
        Serial.print(" The time is unreliable.");     //  Выводим информацию о недостоверном времени.
    }                                             //
    if (gps.errDat) {                             //
        Serial.print(" The date is unreliable.");      //  Выводим информацию о недостоверной дате.
    }                                             //
//   Завершаем строку:                             //
    Serial.print("\r\n");                         //
}