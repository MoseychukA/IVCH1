#include <SoftwareSerial.h>
#include <Wire.h>                                     // подключаем библиотеку для работы с шиной I2C
#include <iarduino_I2C_connect.h>                     // подключаем библиотеку для соединения arduino по шине I2C

//#include <ETH.h>
//#include <SPI.h>

#if !( defined(ESP32) )
#error This code is designed for (SP32_S2/3, ESP32_C3 + W5500) to run on ESP32 platform! Please check your Tools->Board setting.
#endif

#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial

// Debug Level from 0 to 4
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3

// Must connect INT to GPIOxx or not working
#define INT_GPIO            1

#define MISO_GPIO           13
#define MOSI_GPIO           11
#define SCK_GPIO            12
#define CS_GPIO             14

//////////////////////////////////////////////////////////

#include <WebServer_ESP32_SC_W5500.h>

WebServer server(80);

// Enter a MAC address and IP address for your controller below.
#define NUMBER_OF_MAC      20

byte mac[][NUMBER_OF_MAC] =
{
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x02 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x03 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x04 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x05 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x06 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x07 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x08 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x09 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0A },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0B },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0C },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0D },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0E },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0F },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x10 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x11 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x12 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x13 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x14 },
};

// Select the IP address according to your local network
IPAddress myIP(192, 168, 1, 177);
IPAddress myGW(192, 168, 1, 1);
IPAddress mySN(255, 255, 255, 0);

// Google DNS Server IP
IPAddress myDNS(8, 8, 8, 8);

int reqCount = 0;                // number of requests received

void handleRoot()
{
#define BUFFER_SIZE     400

    char temp[BUFFER_SIZE];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;
    int day = hr / 24;

    hr = hr % 24;

    snprintf(temp, BUFFER_SIZE - 1,
        "<html>\
<head>\
<meta http-equiv='refresh' content='5'/>\
<title>AdvancedWebServer %s</title>\
<style>\
body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
</style>\
</head>\
<body>\
<h2>Hi from WebServer_ESP32_SC_W5500!</h2>\
<h3>on %s</h3>\
<p>Uptime: %d d %02d:%02d:%02d</p>\
<img src=\"/test.svg\" />\
</body>\
</html>", BOARD_NAME, BOARD_NAME, day, hr % 24, min % 60, sec % 60);

    server.send(200, F("text/html"), temp);
}

void handleNotFound()
{
    String message = F("File Not Found\n\n");

    message += F("URI: ");
    message += server.uri();
    message += F("\nMethod: ");
    message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
    message += F("\nArguments: ");
    message += server.args();
    message += F("\n");

    for (uint8_t i = 0; i < server.args(); i++)
    {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, F("text/plain"), message);
}

void drawGraph()
{
    String out;
    out.reserve(3000);
    char temp[70];

    out += F("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"310\" height=\"150\">\n");
    out += F("<rect width=\"310\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"2\" stroke=\"rgb(0, 0, 0)\" />\n");
    out += F("<g stroke=\"blue\">\n");
    int y = rand() % 130;

    for (int x = 10; x < 300; x += 10)
    {
        int y2 = rand() % 130;
        sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"2\" />\n", x, 140 - y, x + 10, 140 - y2);
        out += temp;
        y = y2;
    }

    out += F("</g>\n</svg>\n");

    server.send(200, F("image/svg+xml"), out);
}





// Set this to 1 to enable dual Ethernet support
//#define USE_TWO_ETH_PORTS 0
//
//
//
//#ifndef ETH_PHY_CS
//#define ETH_PHY_TYPE ETH_PHY_W5500
//#define ETH_PHY_ADDR 1
//#define ETH_PHY_CS   14
//#define ETH_PHY_IRQ  1
//#define ETH_PHY_RST  2
//#endif
//
//// SPI pins
//#define ETH_SPI_SCK  12
//#define ETH_SPI_MISO 13
//#define ETH_SPI_MOSI 11
//
//static bool eth_connected = false;
//
//void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
//    switch (event) {
//    case ARDUINO_EVENT_ETH_START:
//        Serial.println("ETH Started");
//        //set eth hostname here
//        ETH.setHostname("esp32-eth0");
//        break;
//    case ARDUINO_EVENT_ETH_CONNECTED: Serial.println("ETH Connected"); break;
//    case ARDUINO_EVENT_ETH_GOT_IP:    Serial.printf("ETH Got IP: '%s'\n", esp_netif_get_desc(info.got_ip.esp_netif)); Serial.println(ETH);
//#if USE_TWO_ETH_PORTS
//        Serial.println(ETH1);
//#endif
//        eth_connected = true;
//        break;
//    case ARDUINO_EVENT_ETH_LOST_IP:
//        Serial.println("ETH Lost IP");
//        eth_connected = false;
//        break;
//    case ARDUINO_EVENT_ETH_DISCONNECTED:
//        Serial.println("ETH Disconnected");
//        eth_connected = false;
//        break;
//    case ARDUINO_EVENT_ETH_STOP:
//        Serial.println("ETH Stopped");
//        eth_connected = false;
//        break;
//    default: break;
//    }
//}
//
//void testClient(const char* host, uint16_t port) 
//{
//    Serial.print("\nconnecting to ");
//    Serial.println(host);
//
//    NetworkClient client;
//    if (!client.connect(host, port)) {
//        Serial.println("connection failed");
//        return;
//    }
//    client.printf("GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
//    while (client.connected() && !client.available());
//    while (client.available()) {
//        Serial.write(client.read());
//    }
//
//    Serial.println("closing connection\n");
//    client.stop();
//}
//


iarduino_I2C_connect I2C2;                            // объявляем объект I2C2 для работы c библиотекой iarduino_I2C_connect
byte           REG_Array[32];                         // объявляем массив, данные которого будут доступны мастеру (для чтения/записи) по шине I2C


// As both during TX and RX softSerial is disabling interrupts
// you can not send data from RX to TX on a same ESP32
// left the original example in just to show how to use

// RX = pin 14, TX = 12, none-invert, buffersize 256.
SoftwareSerial swSer(17, 18, false, 256);
//SoftwareSerial swSer(14, 12, false, 256);


void setup() {
  Serial.begin(115200);
  swSer.begin(115200);

  Serial.println("\nSoftware serial test started");

  Wire.begin(0x01);                                   // инициируем подключение к шине I2C в качестве ведомого (slave) устройства, с указанием своего адреса на шине.
  I2C2.begin(REG_Array);                              // инициируем возможность чтения/записи данных по шине I2C, из/в указываемый массив


  for (char ch = ' '; ch <= 'z'; ch++) {
    swSer.write(ch);
  }
  swSer.println("");

  /*Network.onEvent(onEvent);

  SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI);*/
  Serial.print(F("\nStart AdvancedWebServer on "));
  Serial.print(ARDUINO_BOARD);
  Serial.print(F(" with "));
  Serial.println(SHIELD_TYPE);
  Serial.println(WEBSERVER_ESP32_SC_W5500_VERSION);

  ET_LOGWARN(F("Default SPI pinout:"));
  ET_LOGWARN1(F("SPI_HOST:"), ETH_SPI_HOST);
  ET_LOGWARN1(F("MOSI:"), MOSI_GPIO);
  ET_LOGWARN1(F("MISO:"), MISO_GPIO);
  ET_LOGWARN1(F("SCK:"), SCK_GPIO);
  ET_LOGWARN1(F("CS:"), CS_GPIO);
  ET_LOGWARN1(F("INT:"), INT_GPIO);
  ET_LOGWARN1(F("SPI Clock (MHz):"), SPI_CLOCK_MHZ);
  ET_LOGWARN(F("========================="));

  ///////////////////////////////////

  // To be called before ETH.begin()
  ESP32_W5500_onEvent();

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;

  //bool begin(int MISO_GPIO, int MOSI_GPIO, int SCLK_GPIO, int CS_GPIO, int INT_GPIO, int SPI_CLOCK_MHZ,
  //           int SPI_HOST, uint8_t *W5500_Mac = W5500_Default_Mac);
  //ETH.begin( MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST );
  ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac[index]);

  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
  //ETH.config(myIP, myGW, mySN, myDNS);

  ESP32_W5500_waitForConnect();

  ///////////////////////////////////

  server.on(F("/"), handleRoot);
  server.on(F("/test.svg"), drawGraph);
  server.on(F("/inline"), []()
      {
          server.send(200, F("text/plain"), F("This works as well"));
      });

  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print(F("HTTP EthernetWebServer is @ IP : "));
  Serial.println(ETH.localIP());
}

void heartBeatPrint()
{
    static int num = 1;

    Serial.print(F("."));

    if (num == 80)
    {
        Serial.println();
        num = 1;
    }
    else if (num++ % 10 == 0)
    {
        Serial.print(F(" "));
    }
}

void check_status()
{
    static unsigned long checkstatus_timeout = 0;

#define STATUS_CHECK_INTERVAL     10000L

    // Send status report every STATUS_REPORT_INTERVAL (60) seconds: we don't need to send updates frequently if there is no status change.
    if ((millis() > checkstatus_timeout) || (checkstatus_timeout == 0))
    {
        heartBeatPrint();
        checkstatus_timeout = millis() + STATUS_CHECK_INTERVAL;
    }
}


void loop() {
  while (swSer.available() > 0) {
    Serial.write(swSer.read());
  }
  while (Serial.available() > 0) {
    swSer.write(Serial.read());
  }

  server.handleClient();
  check_status();


  //if (eth_connected) {
  //    testClient("google.com", 80);
  //}
  //delay(10000);
}
/*
#include <EthernetWebServer.h>
#include <Ethernet_Generic.h>


// CS pin for the W5500 ethernent shield and frequency
#define W5500_CS  39
#define SPI_FREQ  32000000

// MAC and IP address for the Ethernet communication
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(10, 10, 225, 210);

// (port 80 is default for HTTP):
EthernetWebServer server(80);

void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  while(!Serial);


  SPI.setFrequency(SPI_FREQ);
  Ethernet.init(W5500_CS);
  delay(2000);

  // Start the ethernet connection and the server:
  Ethernet.begin(mac, ip);
  Serial.println("Ethernet has begun!");

  delay(500);

  if (Ethernet.hardwareStatus() == EthernetNoHardware)
  {
    Serial.println("No Ethernet found");

    while (true)
    {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }

  if (Ethernet.linkStatus() == 2)
  {
    Serial.println("No Ethernet cable");
  }

  delay(1000);

  server.begin();
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());

}

void loop() {
  // put your main code here, to run repeatedly:

}

*/