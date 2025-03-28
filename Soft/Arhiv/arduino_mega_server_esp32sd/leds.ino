/*
  Module LED's
  part of Arduino Mega Server project
*/

#ifdef FEATURE_LEDS

#include "esp32-hal-ledc.h"

#define LED_WELLOW_PIN   4
#define LED_RED_PIN      21
#define LED_GREEN_PIN    22

// RGB LEDS

#define R 0
#define G 1
#define B 2

byte rgbPins[] = {32, 27, 33}; // RGB pins

#define R_CH  1  // channel 0-15
#define G_CH  2
#define B_CH  3
#define RGB_LED_RES  8  // resolution 1-16 bits
#define RGB_LED_FREQ 50 // freq limits depend on resolution

// analog show
byte rgbStep = 1;
int rgbDelay = 50;
int rgbR = -1;
int rgbG = 0;
int rgbB = 255;
unsigned long rgbLast = millis();

// Self LED

#define LED_SELF_PIN     2
#define LED_SELF_CHANNEL 0  // 0..15 
#define LED_SELF_RES     8  // resolution 1-16 bits
#define LED_SELF_FREQ    50 // freq limits depend on resolution
#define LED_SELF_ON      40 // 0..256 (8-bit)
#define LED_SELF_OFF     5  // 0..256 (8-bit)

void initLeds() {
  pinMode(LED_WELLOW_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN,  OUTPUT);
  pinMode(LED_RED_PIN,    OUTPUT);
  pinMode(LED_SELF_PIN,   OUTPUT);

  // self
  ledcSetup(LED_SELF_CHANNEL, LED_SELF_FREQ, LED_SELF_RES);
  ledcAttachPin(LED_SELF_PIN, LED_SELF_CHANNEL);
  
  wellowOn();
  greenOff();
  redOff();
  selfOff();

  // RGB
  pinMode(rgbPins[R], OUTPUT); 
  pinMode(rgbPins[G], OUTPUT);
  pinMode(rgbPins[B], OUTPUT);

  ledcSetup(R_CH, RGB_LED_FREQ, RGB_LED_RES);
  ledcSetup(G_CH, RGB_LED_FREQ, RGB_LED_RES);
  ledcSetup(B_CH, RGB_LED_FREQ, RGB_LED_RES);
  
  ledcAttachPin(rgbPins[R], R_CH);
  ledcAttachPin(rgbPins[G], G_CH);
  ledcAttachPin(rgbPins[B], B_CH);
  
  //black(led1);
  moduleLeds = ENABLE;
  started(F("LED"), true);
}

void wellowOn()  {digitalWrite(LED_WELLOW_PIN, HIGH);}
void wellowOff() {digitalWrite(LED_WELLOW_PIN, LOW);}
void greenOn()   {digitalWrite(LED_GREEN_PIN,  HIGH);}
void greenOff()  {digitalWrite(LED_GREEN_PIN,  LOW);}
void redOn()     {digitalWrite(LED_RED_PIN,    HIGH);}
void redOff()    {digitalWrite(LED_RED_PIN,    LOW);}

void yellowControl() {
  if (digitalRead(LED_WELLOW_PIN)) {
    //wellowOff();
  } else {
      wellowOn();
    }
}

void greenControl() {
  if (digitalRead(LED_GREEN_PIN)) {
    greenOff();
  } else {
      greenOn();
    }
}

void redControl() {
  if (digitalRead(LED_RED_PIN)) {
    redOff();
  } else {
      redOn();
    }
}

// Self LED

bool toggle = false;

void selfOn() {
  toggle = true;
  ledcWrite(LED_SELF_CHANNEL, LED_SELF_ON);
}

void selfOff() {
  toggle = false;
  ledcWrite(LED_SELF_CHANNEL, LED_SELF_OFF);
}

void selfControl() {
  if (toggle) {
    selfOff();
  } else {
      selfOn();
    }
}

// RGB LED's

void black  ()       {ledcWrite(R_CH, 0); ledcWrite(G_CH, 0); ledcWrite(B_CH, 0);}
void white  (byte v) {ledcWrite(R_CH, v); ledcWrite(G_CH, v); ledcWrite(B_CH, v);}
void red    (byte v) {ledcWrite(R_CH, v); ledcWrite(G_CH, 0); ledcWrite(B_CH, 0);}
void green  (byte v) {ledcWrite(R_CH, 0); ledcWrite(G_CH, v); ledcWrite(B_CH, 0);}
void blue   (byte v) {ledcWrite(R_CH, 0); ledcWrite(G_CH, 0); ledcWrite(B_CH, v);}
void yellow (byte v) {ledcWrite(R_CH, v); ledcWrite(G_CH, v); ledcWrite(B_CH, 0);}
void magenta(byte v) {ledcWrite(R_CH, v); ledcWrite(G_CH, 0); ledcWrite(B_CH, v);}
void cyan   (byte v) {ledcWrite(R_CH, 0); ledcWrite(G_CH, v); ledcWrite(B_CH, v);}
void color  (byte r, byte g, byte b)
                     {ledcWrite(R_CH, r); ledcWrite(G_CH, g); ledcWrite(B_CH, b);}

void analogShow() {
  if (millis() - rgbLast > rgbDelay) {
    switch (rgbStep) {
      case 1: rgbR++; if (rgbR == 255) {rgbStep = 2;} break; // blue - magenta
      case 2: rgbB--; if (rgbB ==   0) {rgbStep = 3;} break; // magenta - red
      case 3: rgbG++; if (rgbG == 255) {rgbStep = 4;} break; // red - yellow
      case 4: rgbR--; if (rgbR ==   0) {rgbStep = 5;} break; // yellow - green
      case 5: rgbB++; if (rgbB == 255) {rgbStep = 6;} break; // green - cyan
      case 6: rgbG--; if (rgbG ==   0) {rgbStep = 1;} break; // cyan - blue
    }
    ledcWrite(R_CH, rgbR);
    ledcWrite(G_CH, rgbG);
    ledcWrite(B_CH, rgbB);  
    rgbLast = millis();
  }
}

#endif // FEATURE_LEDS
