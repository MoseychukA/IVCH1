/*
  Module nooLite / MR1132
  part of Arduino Mega Server project
*/

#ifdef MR1132_FEATURE

#define TX2 16 // Serial2 TX PIN (to RX noolite)
#define RX2 17 // Serial2 RX PIN (to TX noolite)

// MR1132 channels (0..31)
#define MR_CHANNEL_1 0
#define MR_CHANNEL_2 1
#define MR_CHANNEL_3 2
#define MR_CHANNEL_4 3
#define MR_CHANNEL_5 4

void mr1132Init() {
  Serial2.begin(9600);
  moduleMr1132 = MODULE_ENABLE;
  started("MR1132");
}

// nooLite MR1132 data
byte mrBuf[8];
int mrTogl =         -1;
int mrBindState =    -1;
int mrReceiveBit =   -1;
int mrChannel =      -1;
int mrCommand =      -1;
int mrFormat =       -1;
int mrData0 =        -1;
int mrData1 =        -1;
int mrData2 =        -1;
int mrData3 =        -1;
int mrDeviceType =   -1;
int mrBatteryState = -1;
int mrHumidity =     -1;
int mrBrightness =   -1;
float mrTemp =       -1.0;

// nooLite sensor1 PT112
int ns1BatteryState = -1;
float ns1Temp =       -1.0;

// nooLite sensor2 PT111
int ns2BatteryState = -1;
int ns2Humidity =     -1;
float ns2Temp =       -1.0;

// nooLite pir1 PM111
int np1State =        -1;

// nooLite MR1132 bind/unbind

void mrSerialChannel(byte ch) {
  Serial.println(String(ch));
}

void mrSerial2Channel(byte ch) {
  String s = "";
  if (ch > -1 && ch < 10) {s = "0";}
  Serial2.print(s + String(ch));
}

void mrPrintBind(byte ch) {
  Serial.print("Bind on channel ");
  mrSerialChannel(ch);
}

void mrBind(byte ch) {
  mrPrintBind(ch);
  Serial2.print(F("bind_mode_cell_"));
  mrSerial2Channel(ch);
  Serial2.write(3); // End of Text - B00000011(BIN)
}

void mrPrintUnbind(byte ch) {
  Serial.println(F("Unbind on channel "));
  mrSerialChannel(ch);
}

void mrUnbind(byte ch) {
  mrPrintUnbind(ch);
  Serial2.print(F("clear_one_cell_"));
  mrSerial2Channel(ch);
  Serial2.write(3);
}

void mrBindStop() {
  Serial.println(F("Bind mode off"));
  Serial2.print(F("bind_mode_off"));
  Serial2.write(3);
}

void mrClearAll() {
  Serial.println(F("Clear all cell"));
  Serial2.print(F("clear_all_cell"));
  Serial2.write(3);
}

// nooLite MR1132 print works

void mrPrintHeader() {
  Serial.println();
}

void mrPrintDeviceType() {
  Serial.print(F("Device: ")); 
  if (mrDeviceType == 1) {
    Serial.println(F("PT112"));
  }
  if (mrDeviceType == 2) {
    Serial.println(F("PT111"));
  }
}

void mrPrintBindState() {
  if (mrBindState == 1) {
    Serial.print(F("Bind State: "));
    Serial.println(F("ON"));
  }
}

void mrPrintBatteryState() {
  if (mrBatteryState == 1) {
    Serial.print(F("Battery State: "));
    Serial.println(F("LOW!"));
  }
}

void mrPrintChannel() {
  Serial.print(F("Channel: "));
  Serial.println(mrChannel);
}

void mrPrintTemperature() {
  Serial.print(F("Temp: "));
  Serial.println(mrTemp);
}

void mrPrintHumidity() {
  Serial.print(F("Humidity: "));
  Serial.println(mrHumidity);
}

// nooLite MR1132 data works

void mrClearData() {
  mrTogl =         -1;
  mrBindState =    -1;
  mrReceiveBit =   -1;
  mrChannel =      -1;
  mrCommand =      -1;
  mrFormat =       -1;
  mrData0 =        -1;
  mrData1 =        -1;
  mrData2 =        -1;
  mrData3 =        -1;
  mrDeviceType =   -1;
  mrBatteryState = -1;
  mrHumidity =     -1;
  mrBrightness =   -1;
  mrTemp =         -1.0;
}

void ns1Clear() {
  ns1BatteryState = -1;
  ns1Temp = -1.0;
}

void ns2Clear() {
  ns2BatteryState = -1;
  ns2Humidity = -1;
  ns2Temp = -1.0;
}

void np1Clear() {
  np1State = -1;
}

void ns1Set() {
  ns1BatteryState = mrBatteryState;
  ns1Temp = mrTemp;
}

void ns2Set() {
  ns2BatteryState = mrBatteryState;
  ns2Humidity = mrHumidity;
  ns2Temp = mrTemp;
}

void np1Set() {
  np1State = mrCommand;
}

void mrSetTogl() {
  byte b0 = bitRead(mrBuf[0], 0);
  byte b1 = bitRead(mrBuf[0], 1);
  byte b2 = bitRead(mrBuf[0], 2);
  byte b3 = bitRead(mrBuf[0], 3);
  byte b4 = bitRead(mrBuf[0], 4);
  byte b5 = bitRead(mrBuf[0], 5);
  mrTogl = 32*b5 + 16*b4 + 8*b3 + 4*b2 + 2*b1 + b0;
}

void mrSetBindState() {
  mrBindState = bitRead(mrBuf[0], 6);
}

void mrSetReceiveBit() {
  mrReceiveBit = bitRead(mrBuf[0], 7);
}

void mrSetChannel() {
  mrChannel = mrBuf[1];
}

void mrSetCommand() {
  mrCommand = mrBuf[2];
}

void mrSetFormat() {
  mrFormat = mrBuf[3];
}

void mrSetDeviceType() {
  byte tp1 = bitRead(mrBuf[5], 4);
  byte tp2 = bitRead(mrBuf[5], 5);
  byte tp3 = bitRead(mrBuf[5], 6);
  mrDeviceType = 4*tp3 + 2*tp2 + tp1;
}

void mrSetDatas() {
  mrData0 = mrBuf[4];
  mrData1 = mrBuf[5];
  mrData2 = mrBuf[6];
  mrData3 = mrBuf[7];
}

void mrSetTemperature() {
  byte t8 = bitRead(mrData1, 0);
  byte t9 = bitRead(mrData1, 1);
  byte t10= bitRead(mrData1, 2);
  int temp2 = 1024*t10 + 512*t9 + 256*t8;

  int temp = mrData0 + temp2;

  byte t11 = bitRead(mrData1, 3);
  if (t11 == 1) {
    temp = (4096 - temp) * -1;
  }
  mrTemp = (float)temp / 10.0;
}

void mrSetBatteryState() {
  mrBatteryState = bitRead(mrBuf[5], 7);
}

void mrSetHumidity() {
  mrHumidity = mrData2;
}

void mrSetBrightness() {
  mrBrightness = mrData3;
}

void mrNewData() {
  mrPrintHeader();
  
  mrSetBindState();
  mrPrintBindState();
  mrSetChannel();
  mrPrintChannel();

  mrSetCommand();
  mrSetDatas();

  switch (mrCommand) {
    case 0:
      np1Clear();
      Serial.print(F("PIR command: "));
      Serial.println(F("OFF"));
      np1Set();
      break;
    case 2:
      np1Clear();
      Serial.print(F("PIR command: "));
      Serial.println(F("ON"));
      np1Set();
      break;
    case 21:
      mrSetDeviceType();
      mrPrintDeviceType();
      if (mrDeviceType == 1) {
        ns1Clear();
        mrSetTemperature();
        mrPrintTemperature();
        mrSetBatteryState();
        ns1Set();
      }
      if (mrDeviceType == 2) {
        ns2Clear();
        mrSetTemperature();
        mrPrintTemperature();
        mrSetHumidity();
        mrPrintHumidity();
        mrSetBatteryState();
        ns2Set();
      }
      break;
    default: 
      ;
  } // switch
  mrSetBatteryState();
} // newData()

void mr1132Work() {
  byte bytes = Serial2.available();
  if (bytes > 0 && bytes != 4) {
    Serial.println(bytes);
  }
  if (bytes >= 8) {
    mrBuf[0] = Serial2.read();
    mrBuf[1] = Serial2.read();
    mrBuf[2] = Serial2.read();
    mrBuf[3] = Serial2.read();
    if (mrBuf[0] == 79 && mrBuf[1] == 75 && mrBuf[2] == 13 && mrBuf[3] == 10) {
      Serial.println("OK");
    } else {
        mrBuf[4] = Serial2.read();
        mrBuf[5] = Serial2.read();
        mrBuf[6] = Serial2.read();
        mrBuf[7] = Serial2.read();
        mrNewData();
      }
  }
}

#endif // MR1132_FEATURE
