 
#include "defines.h"
#include <STM32RTC.h>
#include <Wire.h>                                     // подключаем библиотеку для работы с шиной I2C
//#include <iarduino_I2C_connect.h>                     // подключаем библиотеку для соединения arduino по шине I2C
//#include <iarduino_GPS_NMEA.h>                        //  Подключаем библиотеку для расшифровки строк протокола NMEA получаемых по UART.
//#include <iarduino_GPS_ATGM336.h>                    //  Подключаем библиотеку для настройки параметров работы GPS модуля ATGM336.
#include <EEPROM.h>
#include <SPI.h>



#define debug true // для вывода отладочных сообщений

EthernetWebServer server(80);

void handleRoot()
{
    server.send(200, "text/plain", String("Hello from EthernetWebServer on ") + BOARD_NAME);
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
}





#include <HardwareSerial.h>
//Hardware serial #1: RX = digital pin 10, TX = digital pin 11
HardwareSerial SerialRS2321(PA10, PA9);    //USART1
HardwareSerial SerialRS2322(PC11, PC10);   //USART4
HardwareSerial SerialGPS(PA3, PA2);        //USART2
HardwareSerial SerialRS4851(PB11, PB12);   //USART3
HardwareSerial SerialRS4852(PD2, PC12);    //USART5


void setup() 
{
  Serial.begin(115200);

  SerialRS2321.begin(9600);
  SerialRS2322.begin(9600);
  SerialGPS.begin(9600);
  SerialRS4851.begin(9600);
  SerialRS4852.begin(9600);
  while (!Serial && millis() < 5000);
  delay(3000);

  Serial.println("\nStart HelloServer2 on " + String(BOARD_NAME) + ", using " + String(SHIELD_TYPE));

#if USE_ETHERNET_GENERIC
  Serial.println(ETHERNET_GENERIC_VERSION);
#endif

  Serial.println(ETHERNET_WEBSERVER_STM32_VERSION);

#if !(USE_BUILTIN_ETHERNET)

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("MOSI:"), MOSI);
  ET_LOGWARN1(F("MISO:"), MISO);
  ET_LOGWARN1(F("SCK:"), SCK);
  ET_LOGWARN1(F("SS:"), SS);
  ET_LOGWARN(F("========================="));
#endif

#if !(USE_BUILTIN_ETHERNET || USE_UIP_ETHERNET)
  // For other boards, to change if necessary
#if ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
  Ethernet.init(USE_THIS_SS_PIN);
#endif  //( ( USE_ETHERNET_GENERIC || USE_ETHERNET_ENC )
#endif

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP
  Ethernet.begin(mac[index], ip);
 // Ethernet.begin(mac[index]);

  Serial.print(F("Connected! IP address: "));
  Serial.println(Ethernet.localIP());

  server.on("/", handleRoot);

  server.on("/inline", []()
      {
          server.send(200, "text/plain", "This works as well");
      });

  server.on("/gif", []()
      {
          static const uint8_t gif[] =
          {
            0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
            0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
            0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
            0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
            0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
          };
          char gif_colored[sizeof(gif)];

          memcpy(gif_colored, gif, sizeof(gif));

          // Set the background to a random set of colors
          gif_colored[16] = millis() % 256;
          gif_colored[17] = millis() % 256;
          gif_colored[18] = millis() % 256;

          server.send(200, (char*)"image/gif", gif_colored, sizeof(gif_colored));
      });

  server.onNotFound(handleNotFound);

  server.begin();

  Serial.print("HTTP EthernetWebServer started @ IP : ");
  Serial.println(Ethernet.localIP());
}

void loop()
{
    server.handleClient();
}

