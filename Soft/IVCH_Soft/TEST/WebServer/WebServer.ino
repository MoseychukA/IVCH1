/****************************************************************************************************************************
  WebServer.ino

  Ethernet_Generic is a library for the W5x00 Ethernet shields trying to merge the good features of
  previous Ethernet libraries

  Built by Khoi Hoang https://github.com/khoih-prog/Ethernet_Generic
 *****************************************************************************************************************************/
/*
   The Arduino board communicates with the shield using the SPI bus. This is on digital pins 11, 12, and 13 on the Uno
   and pins 50, 51, and 52 on the Mega. On both boards, pin 10 is used as SS. On the Mega, the hardware SS pin, 53,
   is not used to select the Ethernet controller chip, but it must be kept as an output or the SPI interface won't work.
*/

#include "defines.h"

int reqCount = 0;                // number of requests received

EthernetServer server(80);

void setup()
{
  SerialDebug.begin(115200);

  while (!Serial && millis() < 5000);
  delay(2000);
  SerialDebug.print("\nStarting WebServer on ");
  //SerialDebug.print(BOARD_NAME);
  SerialDebug.print(F(" with "));
  SerialDebug.println(SHIELD_TYPE);
  SerialDebug.println(ETHERNET_GENERIC_VERSION);

#if (USING_SPI2)
#if defined(CUR_PIN_MISO)
  ETG_LOGWARN(F("Default SPI pinout:"));
  ETG_LOGWARN1(F("MOSI:"), CUR_PIN_MOSI);
  ETG_LOGWARN1(F("MISO:"), CUR_PIN_MISO);
  ETG_LOGWARN1(F("SCK:"),  CUR_PIN_SCK);
  ETG_LOGWARN1(F("SS:"),   CUR_PIN_SS);
  ETG_LOGWARN(F("========================="));
#endif
#else
  ETG_LOGWARN(F("Default SPI pinout:"));
  ETG_LOGWARN1(F("MOSI:"), MOSI);
  ETG_LOGWARN1(F("MISO:"), MISO);
  ETG_LOGWARN1(F("SCK:"),  SCK);
  ETG_LOGWARN1(F("SS:"),   SS);
  ETG_LOGWARN(F("========================="));
#endif


  // For other boards, to change if necessary
  Ethernet.init (USE_THIS_SS_PIN);

  // start the ethernet connection and the server:
  // Use DHCP dynamic IP and random mac
  uint16_t index = millis() % NUMBER_OF_MAC;
  // Use Static IP
 // Ethernet.begin(mac[index], ip);
  Ethernet.begin(mac[index]);

  //SPIClass SPI2(HSPI);
  //Ethernet.begin(mac[index], &SPI2);

  // Just info to know how to connect correctly
  // To change for other SPI
#if defined(CUR_PIN_MISO)
  ETG_LOGWARN(F("Currently Used SPI pinout:"));
  ETG_LOGWARN1(F("MOSI:"), CUR_PIN_MOSI);
  ETG_LOGWARN1(F("MISO:"), CUR_PIN_MISO);
  ETG_LOGWARN1(F("SCK:"),  CUR_PIN_SCK);
  ETG_LOGWARN1(F("SS:"),   CUR_PIN_SS);
  ETG_LOGWARN(F("========================="));
#else
  ETG_LOGWARN(F("Currently Used SPI pinout:"));
  ETG_LOGWARN1(F("MOSI:"), MOSI);
  ETG_LOGWARN1(F("MISO:"), MISO);
  ETG_LOGWARN1(F("SCK:"),  SCK);
  ETG_LOGWARN1(F("SS:"),   SS);
  ETG_LOGWARN(F("========================="));
#endif

  SerialDebug.print(F("Using mac index = "));
  SerialDebug.println(index);

  SerialDebug.print(F("Connected! IP address: "));
  SerialDebug.println(Ethernet.localIP());

  if ( (Ethernet.getChip() == w5500) || (Ethernet.getChip() == w6100) || (Ethernet.getAltChip() == w5100s) )
  {
    if (Ethernet.getChip() == w6100)
      SerialDebug.print(F("W6100 => "));
    else if (Ethernet.getChip() == w5500)
      SerialDebug.print(F("W6100 => "));
    else
      SerialDebug.print(F("W5100S => "));
    
    SerialDebug.print(F("Speed: "));
    SerialDebug.print(Ethernet.speedReport());
    SerialDebug.print(F(", Duplex: "));
    SerialDebug.print(Ethernet.duplexReport());
    SerialDebug.print(F(", Link status: "));
    SerialDebug.println(Ethernet.linkReport());
  }

  // start the web server on port 80
  server.begin();
}

void loop()
{
  // listen for incoming clients
  EthernetClient client = server.available();

  if (client)
  {
    SerialDebug.println(F("New client"));
    // an http request ends with a blank line
    bool currentLineIsBlank = true;

    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        SerialDebug.write(c);

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank)
        {
          SerialDebug.println(F("Sending response"));

          // send a standard http response header
          // use \r\n instead of many println statements to speedup data send
          client.print(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"  // the connection will be closed after completion of the response
            "Refresh: 20\r\n"        // refresh the page automatically every 20 sec
            "\r\n");
          client.print("<!DOCTYPE HTML>\r\n");
          client.print("<html>\r\n");
          client.print("<h2>Hello World from ");
          client.print(BOARD_NAME);
          client.print("!</h2>\r\n");
          client.print("Requests received: ");
          client.print(++reqCount);
          client.print("<br>\r\n");
          client.print("Analog input A0: ");
          client.print(analogRead(0));
          client.print("<br>\r\n");
          client.print("</html>\r\n");
          break;
        }

        if (c == '\n')
        {
          // you're starting a new line
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }

    // give the web browser time to receive the data
    delay(10);

    // close the connection:
    client.stop();
    SerialDebug.println(F("Client disconnected"));
  }
}
