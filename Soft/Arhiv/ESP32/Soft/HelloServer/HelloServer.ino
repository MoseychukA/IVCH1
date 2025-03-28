/*
 * HelloServer example from the ESP32 WebServer library modified for Ethernet.
 */

#include <EthernetESP32.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <SPI.h>
#include <LoRa.h>

 //определяем номера пинов, используемые трансивером
#define ss 46
#define rst 7
#define dio0 3

int counter = 0;
const int ledPin = 4;//LED_BUILTIN;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED

unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF };
W5500Driver driver(14, 1, 2);
SPIClass SPI1(HSPI);
//W5500Driver driver(14);
//ENC28J60Driver driver;
//EMACDriver driver(ETH_PHY_LAN8720);

WebServer server(80);

const int led = 4;

void handleRoot() {
  digitalWrite(led, 1);
  server.send(200, "text/plain", "hello from esp32!");
  digitalWrite(led, 0);
}

void handleNotFound() {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);

  delay(500);
  while (!Serial);

  SPI1.begin(12, 13, 11);

  Serial.println("LoRa Sender");

  //настраиваем трансивер
  LoRa.setPins(ss, rst, dio0);

  //замените LoRa.begin(---E-) частотой, которую вы собираетесь использовать 
  while (!LoRa.begin(866E6)) 
  {
      Serial.println(".");
      delay(500);
  }
  // Измените слово синхронизации (0xF3)
 // Слово синхронизации нужно, чтобы не получать сообщения от других трансиверов
 // можно изменять в диапазоне 0-0xFF
  LoRa.setSyncWord(0xF3);
  Serial.println("LoRa Initializing OK!");


  driver.setSPI(SPI1);
 /* driver.setSpiFreq(10);
  driver.setPhyAddress(0);*/

  Ethernet.init(driver);

  Serial.println("Initialize Ethernet with DHCP:");

  //if (Ethernet.begin()) 
  //{
  //  Serial.print("  DHCP assigned IP ");
  //  Serial.println(Ethernet.localIP());
  //}
  //else 
  //{
  //  Serial.println("Failed to configure Ethernet using DHCP");
  //  while (true) {
  //    delay(1);
  //  }
  //}

  //if (MDNS.begin("esp32")) 
  //{
  //  Serial.println("MDNS responder started");
  //}

  //server.on("/", handleRoot);

  //server.on("/inline", []() {
  //  server.send(200, "text/plain", "this works as well");
  //});

  //server.onNotFound(handleNotFound);

  //server.begin();
  //Serial.println("HTTP server started");
}

void loop(void) 
{
  //server.handleClient();
  //delay(2);  //allow the cpu to switch to other tasks


    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        Serial.print("Sending packet: ");
        Serial.println(counter);

        // send packet
        LoRa.beginPacket();
        LoRa.print("hello1 ");
        LoRa.print(counter);
        LoRa.endPacket();


        if (ledState == LOW) {
            ledState = HIGH;
        }
        else {
            ledState = LOW;
        }

        // set the LED with the ledState of the variable:
        digitalWrite(ledPin, ledState);

        counter++;

    }


}
