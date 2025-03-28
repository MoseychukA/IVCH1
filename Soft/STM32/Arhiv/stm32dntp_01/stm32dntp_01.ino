#include "sntp_utils.h"
#include "commands.h"
#include <libmaple/iwdg.h>
#include <RTClock.h>

uint32_t micros_offset = 0;
uint32_t uptime_sec_count = 0;

/* ModbusIP object */
ModbusIP mb;

int mb_begin_offset = 0;
extern RTClock rtclock;

/*Default ip*/
uint8_t myip[] = { 192,168,1,177 };

uint8_t ntp_protocol = true, mbtcp_protocol = true,
        melsec_protocol = false, gps_enabled = true;

void rtc_sec_intr()
{
   if (rtc_is_second()) uptime_sec_count++;
}


const int ledPin = PC13;// the number of the LED pin

// Variables will change:
int ledState = LOW;             // ledState used to set the LED

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated

// constants won't change:
const long interval = 1000;           // interval at which to blink (milliseconds)

int count = 0;

USBSerial SerialUSB;

void setup()
{
   SerialUSB.begin(9600);
   while (!SerialUSB) {
       ; // wait for serial port to connect. Needed for native USB port only
   }

   delay(1000);
   SerialUSB.println("Start dhtp");
   delay(1000);
   pinMode(ledPin, OUTPUT);
   rtc_attach_interrupt(RTC_SECONDS_INTERRUPT, rtc_sec_intr);
   rtclock.setTime(1508417425);
  // iwdg_init(IWDG_PRE_256, 2000); // init watchdog timer
   static byte mymac[] = { 0x24,0x69,0x69,0x2D,0x30,0x1F };

   uint16_t chksum = (uint16_t)EEPROM.read(4) + (uint16_t)EEPROM.read(5)*256;

   /* if chksum is OK use ip from "eeprom" */

   if((chksum == (uint8_t)EEPROM.read(0)+(uint8_t)EEPROM.read(1)+
                 (uint8_t)EEPROM.read(2)+(uint8_t)EEPROM.read(3))
                 &&chksum>4&&chksum<1000)
   {
      uint8_t ip []= { (uint8_t)EEPROM.read(0), (uint8_t)EEPROM.read(1),
                       (uint8_t)EEPROM.read(2), (uint8_t)EEPROM.read(3) };
      memcpy(myip, ip, 4);
   }

   /* add empty modbus registers into modbus object */
   for(int i = 0; i < 15; i++)
   {
      mb.addHreg(i,0);
   }

  // mb.config(mymac, myip);
   //ether.enableBroadcast();
   //micros_offset = micros()%1000000;

   Serial2.begin(9600);  //starts serial for NMEA receiving

   //ether.udpServerListenOnPort(&ntp_recv_and_respond, 123);
   //ether.udpServerListenOnPort(&echo_recv_and_respond, 4800);
   //ether.udpServerListenOnPort(&udp_listen_commands,7800);
   SerialUSB.println("Start End");
}

void loop()
{
  // mb.task();
   processGNRMC(&Serial2);
   check_for_serial_commands((HardwareSerial*)&SerialUSB);
  // fill_modbus_time();
  // iwdg_feed(); //feed watchdog
  //  SerialUSB.println("loop dhtp");
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) 
    {
        // save the last time you blinked the LED
        previousMillis = currentMillis;

        // if the LED is off turn it on and vice-versa:
        if (ledState == LOW) {
            ledState = HIGH;
        }
        else {
            ledState = LOW;
        }

        // set the LED with the ledState of the variable:
        digitalWrite(ledPin, ledState);
        SerialUSB.print("loop dhtp: ");
        SerialUSB.println(count);
        count++;
        if (count == 999)count = 0;
    }
}
