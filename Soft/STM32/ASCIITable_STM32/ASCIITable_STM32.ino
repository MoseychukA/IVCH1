
//
//#include <HardwareSerial.h>
////Hardware serial #1: RX = digital pin 10, TX = digital pin 11
//
//HardwareSerial SerialRS232(PA10, PA9);
//HardwareSerial SerialGPS(PA3, PA2);
//HardwareSerial SerialRS485(PB11, PB12);


#include <SoftwareSerial.h>
////Hardware serial #1: RX = digital pin 10, TX = digital pin 11
//
//SoftwareSerial SerialRS232(PA10, PA9);
//SoftwareSerial SerialGPS(PA3, PA2);
//SoftwareSerial SerialRS485(PB11, PB12);
//SoftwareSerial SerialSIM800C(PD3, PD4);    //USART
//SoftwareSerial SerialSIM800C(PB11, PB12);
SoftwareSerial SerialSIM800C(PA10, PA9);


const int ledPin = PB5;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000;               // interval at which to blink (milliseconds)
int count = 0;





void setup() {
  //Initialize serial and wait for port to open:
  //Serial.begin(115200);
 // Serial1.begin(115200);
 // SerialGPS.begin(9600);
 // SerialRS485.begin(115200);
    SerialSIM800C.begin(57600);

  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}
    pinMode(ledPin, OUTPUT);
  // prints title with ending line break
    SerialSIM800C.println("ASCII Table ~ Character Map");
}

// first visible ASCIIcharacter '!' is number 33:
int thisByte = 33;
// you can also write ASCII characters in single quotes.
// for example, '!' is the same as 33, so you could also use this:
// int thisByte = '!';

void loop() {
  // prints value unaltered, i.e. the raw binary version of the byte.
  // The Serial Monitor interprets all bytes as ASCII, so 33, the first number,
  // will show up as '!'
    SerialSIM800C.write(thisByte);

    SerialSIM800C.print(", dec: ");
  // prints value as string as an ASCII-encoded decimal (base 10).
  // Decimal is the default format for Serial.print() and Serial.println(),
  // so no modifier is needed:
    SerialSIM800C.print(thisByte);
  // But you can declare the modifier for decimal if you want to.
  // this also works if you uncomment it:

  // Serial.print(thisByte, DEC);


    SerialSIM800C.print(", hex: ");
  // prints value as string in hexadecimal (base 16):
    SerialSIM800C.print(thisByte, HEX);

    SerialSIM800C.print(", oct: ");
  // prints value as string in octal (base 8);
    SerialSIM800C.print(thisByte, OCT);

    SerialSIM800C.print(", bin: ");
  // prints value as string in binary (base 2) also prints ending line break:
    SerialSIM800C.println(thisByte, BIN);

  // if printed last visible character '~' or 126, stop:
  if (thisByte == 126) 
  {  
    thisByte = 33;
    if (ledState == LOW)
    {
        ledState = HIGH;
    }
    else
    {
        ledState = LOW;
    }
  }
  // go on to the next character
  thisByte++;
}
