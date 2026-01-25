/*
 Internet2 module
 MCU:STM32F103CBT6
 W5500:SPI1 PA4=SS,PA5=SCK,PA6=MISO,PA7=MOSI
 UART2:PA2/PA3 (clients)
 UART3 RS485:PB10/PB11 (clients,auto-direction)
 I2C Slave:PB6/PB7 @100kHz,addr 0x42
*/

#include <Arduino.h>
#include <Wire.h>

#include "I2CInternet2Proto.h"
#include "W5500NtpServerClient.h"
#include "TimeSerialServices.h"

static const uint8_t I2C_ADDR = 0x42;

W5500NtpServerClient net2;
I2CInternet2Proto proto(net2);

// UART clients
HardwareSerial U2(PA3, PA2);// = Serial2; // PA3 RX,PA2 TX (STM32duino naming may swap; adjust if needed)
HardwareSerial U3(PB11, PB10);// = Serial3; // PB11 RX,PB10 TX

TimeSerialServices serialSvc(U2, U3, net2);

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


void setup() {
	Serial.begin(115200);
	delay(500);

	w5500HardReset();

	//Serial.println("Internet2 slave start");

	// версия
	String ver_soft = __FILE__;
	int val_srt = ver_soft.lastIndexOf('\\');
	if (val_srt >= 0) ver_soft.remove(0, val_srt + 1);
	val_srt = ver_soft.lastIndexOf('.');
	if (val_srt >= 0) ver_soft.remove(val_srt);
	Serial.print("************ ");
	Serial.print(ver_soft);
	Serial.println(" ************");

	Wire.begin(I2C_ADDR);
	Wire.setClock(100000);
	Wire.onReceive(I2CInternet2Proto::onReceiveThunk);
	Wire.onRequest(I2CInternet2Proto::onRequestThunk);
	I2CInternet2Proto::instance = &proto;

	U2.begin(115200);
	U3.begin(115200);

	net2.begin(); // старт Ethernet + UDP
	proto.resetReg(); // по умолчанию STATUS

	Serial.println("Internet2 slave ready");
}

void loop() {
	net2.tick();
	serialSvc.tick();
}