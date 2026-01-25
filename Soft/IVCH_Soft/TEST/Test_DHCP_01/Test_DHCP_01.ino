#include <SPI.h>
#include <Ethernet_Generic.h>

byte mac[] = { 0x02,0x11,0x22,0x33,0x44,0x55 };

// fallback static
IPAddress staticIp(192, 168, 75, 106);
IPAddress staticDns(8, 8, 8, 8);
IPAddress staticGw(192, 168, 75, 1);
IPAddress staticMask(255, 255, 255, 0);

static void printNet()
{
	Serial.print("IP="); Serial.println(Ethernet.localIP());
	Serial.print("GW="); Serial.println(Ethernet.gatewayIP());
	Serial.print("MSK="); Serial.println(Ethernet.subnetMask());
	Serial.print("DNS="); Serial.println(Ethernet.dnsServerIP());
}

static void printLink()
{
	// ¬ариант A (если доступен в вашей версии):
	// Serial.print("LinkStatus="); Serial.println(Ethernet.linkStatus() == LinkON ? "ON" :"OFF/UNK");

	// ¬ариант B (точно есть в Ethernet_Generic,если W5100 объ€влен):
	W5100Linkstatus ls = W5100.getLinkStatus();
	Serial.print("LinkStatus=");
	Serial.println(ls == LINK_ON ? "ON" : (ls == LINK_OFF ? "OFF" : "UNKNOWN"));

	Serial.print("LinkReport="); Serial.println(Ethernet.linkReport());
}

#define W5500_RST_PIN PB1

static void w5500HardReset()
{
	pinMode(W5500_RST_PIN, OUTPUT);
	digitalWrite(W5500_RST_PIN, HIGH);
	delay(10);

	digitalWrite(W5500_RST_PIN, LOW);
	delay(50); // импульс reset

	digitalWrite(W5500_RST_PIN, HIGH);
	delay(200); // дать чипу стартануть
}



void setup()
{
	Serial.begin(115200);
	delay(2000);

	w5500HardReset();

	Serial.println("\n--- DHCP test (Ethernet_Generic / W5500) ---");

	SPI.begin();
	Ethernet.init(PA4); // CS = PA4

	Serial.println("[ETH] DHCP begin...");
	int ok = Ethernet.begin(mac, 3000, 1000); // timeout,responseTimeout

	if (ok)
	{
		Serial.println("[ETH] DHCP OK");
		printNet();
		printLink();
	}
	else
	{
		Serial.println("[ETH] DHCP FAIL -> fallback to static");
		Ethernet.begin(mac, staticIp, staticDns, staticGw, staticMask);
		printNet();
		printLink();
	}
}

void loop()
{
	Ethernet.maintain();

	static uint32_t t0 = 0;
	if (millis() - t0 >= 2000)
	{
		t0 = millis();
		Serial.println("=============================");
		printNet();
		printLink();
	}
}