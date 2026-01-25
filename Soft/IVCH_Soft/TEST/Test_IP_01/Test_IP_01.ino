#include <SPI.h>
#include <Ethernet_Generic.h>

byte mac[] = { 0x02,0x11,0x22,0x33,0x44,0x55 };

IPAddress ip(192, 168, 75, 106);
IPAddress dnsServer(8, 8, 8, 8);
IPAddress gw(192, 168, 75, 1);
IPAddress mask(255, 255, 255, 0);

void setup()
{
	Serial.begin(115200);
	delay(2000);

	SPI.begin();
	Ethernet.init(PA4); // <-- поставьте ваш реальный CS:PA4/PB12/...
	Ethernet.begin(mac, ip, dnsServer, gw, mask);

	Serial.print("IP="); Serial.println(Ethernet.localIP());
	Serial.print("GW="); Serial.println(Ethernet.gatewayIP());
	Serial.print("MSK="); Serial.println(Ethernet.subnetMask());
	Serial.print("DNS="); Serial.println(Ethernet.dnsServerIP());
}

void loop() {}


