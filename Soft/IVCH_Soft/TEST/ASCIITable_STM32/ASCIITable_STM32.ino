


const int ledPin = PC13;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
const long interval = 1000;               // interval at which to blink (milliseconds)
int count = 0;





void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(115200);


  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}
    pinMode(ledPin, OUTPUT);
 
    Serial.println("ASCII Table ~ Character Map");
}

int thisByte = 33;  // first visible ASCIIcharacter '!' is number 33:

void loop() 
{
 
    Serial.write(thisByte);

    Serial.print(", dec: ");

   Serial.print(thisByte, DEC);

    Serial.print(", hex: ");
  // prints value as string in hexadecimal (base 16):
    Serial.print(thisByte, HEX);

    Serial.print(", oct: ");
  // prints value as string in octal (base 8);
    Serial.print(thisByte, OCT);

    Serial.print(", bin: ");
  // prints value as string in binary (base 2) also prints ending line break:
    Serial.println(thisByte, BIN);

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

    digitalWrite(ledPin, ledState);
  }
  // go on to the next character
  thisByte++;
  delay(10);
}
