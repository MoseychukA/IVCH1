/*
  Module nooLite / MT1132
  part of Arduino Mega Server project
*/

#ifdef MT1132_FEATURE

#define PIN_TX 18 // TX PIN (to RX noolite)
#define PIN_RX 19 // RX PIN (to TX noolite)

// MT1132 channels (0..31)
#define NOO_CHANNEL_1 0
#define NOO_CHANNEL_2 1
#define NOO_CHANNEL_3 2
#define NOO_CHANNEL_4 3
#define NOO_CHANNEL_5 4

void mt1132Init() {
  Serial1.begin(9600);
  moduleMt1132 = MODULE_ENABLE;
  started("nooLite");
}

void nooSendCommand(byte channel, byte command, byte data, byte format) {
  byte buf[12];

  for (byte i = 0; i < 12; i++) {
    buf[i] = 0;
  }

  buf[0] = 85;
  buf[1] = B01010000;
  buf[2] = command;
  buf[3] = format;
  buf[5] = channel;
  buf[6] = data;

  int checkSum = 0;
  for (byte i = 0; i < 10; i++) {
    checkSum += buf[i];
  }

  buf[10] = lowByte(checkSum);
  buf[11] = 170;

  for (byte i = 0; i < (12); i++) {
    Serial1.write(buf[i]);
  }
  /*
  Serial.println(">");
  for (byte i = 0; i < (12); i++) {
    Serial.print(buf[i]);
  }
  Serial.println();
  */
}

byte ledLastState[] = {127, 127, 127}; 

void nooSendCommandLed(byte channel, byte command, byte r, byte g, byte b, byte format) {
  byte buf[12];
  
  ledLastState[0] = r;
  ledLastState[1] = g;
  ledLastState[2] = b;

  for (byte i = 0; i < 12; i++) {
    buf[i] = 0;
  }

  buf[0] = 85;
  buf[1] = B01010000;
  buf[2] = command;
  buf[3] = format;
  buf[5] = channel;
  buf[6] = r;
  buf[7] = g;
  buf[8] = b;
  
  int checkSum = 0;
  for (byte i = 0; i < 10; i++) {
    checkSum += buf[i];
  }

  buf[10] = lowByte(checkSum);
  buf[11] = 170;

  for (byte i = 0; i < (12); i++) {
    Serial1.write(buf[i]);
  }
}

//                                     command data format
void nooBind   (byte ch) {nooSendCommand(ch, 15, 0, 0);}
void nooUnbind (byte ch) {nooSendCommand(ch,  9, 0, 0);}

void nooOn     (byte ch) {nooSendCommand(ch,  2, 0, 0);}
void nooOff    (byte ch) {nooSendCommand(ch,  0, 0, 0);}
void nooTrigger(byte ch) {nooSendCommand(ch,  4, 0, 0);}
void nooCancel (byte ch) {nooSendCommand(ch, 10, 0, 0);}

void nooUp     (byte ch) {nooSendCommand(ch,  3, 0, 0);}
void nooDown   (byte ch) {nooSendCommand(ch,  1, 0, 0);}
void nooRevers (byte ch) {nooSendCommand(ch,  5, 0, 0);}

void nooValue  (byte ch, byte v) {nooSendCommand(ch,  6, v, 1);}

void nooLed    (byte ch, byte r, byte g, byte b) {nooSendCommandLed(ch,  6, r, g, b, 3);}
void nooBlack  (byte ch)         {nooSendCommandLed(ch,  6, 0, 0, 0, 3);}
void nooWhite  (byte ch)         {nooSendCommandLed(ch,  6, 255, 255, 255, 3);}
void nooGray   (byte ch, byte v) {nooSendCommandLed(ch,  6, v, v, v, 3);}
void nooRed    (byte ch, byte v) {nooSendCommandLed(ch,  6, v, 0, 0, 3);}
void nooGreen  (byte ch, byte v) {nooSendCommandLed(ch,  6, 0, v, 0, 3);}
void nooBlue   (byte ch, byte v) {nooSendCommandLed(ch,  6, 0, 0, v, 3);}
void nooYellow (byte ch, byte v) {nooSendCommandLed(ch,  6, v, v, 0, 3);}
void nooMagenta(byte ch, byte v) {nooSendCommandLed(ch,  6, v, 0, v, 3);}
void nooCyan   (byte ch, byte v) {nooSendCommandLed(ch,  6, 0, v, v, 3);}

void nooSetLedLastState(byte ch) {nooSendCommandLed(ch,  6, ledLastState[0], ledLastState[1], ledLastState[2], 3);}

void mt1132Work() {

}

#endif // MT1132_FEATURE
