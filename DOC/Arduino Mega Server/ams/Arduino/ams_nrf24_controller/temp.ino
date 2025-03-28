/*
  Modul Temperature
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

#ifdef TEMP_FEATURE

#include <DallasTemperature.h>
#include <OneWire.h>

// DS18B20
#define ONE_WIRE_BUS 4

byte const MAX_TEMP_SENSORS = 1;
byte const MAX_ID_DIGITS    = 7;

char objSens[MAX_TEMP_SENSORS][12] = {
  "Temp1", 
  //"Temp2", 
  //"Temp3",
};

byte idTempDat[MAX_TEMP_SENSORS][MAX_ID_DIGITS] = { 
  {40, 226, 129, 102, 4, 0, 0}, // Sensor1 ID
//{40, 221,  16,  81, 3, 0, 0}, // Sensor2 ID
//{40,  23,  98,  87, 3, 0, 0}, // Sensor3 ID
}; 

float currentTemp[MAX_TEMP_SENSORS];

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempSensor[MAX_TEMP_SENSORS];

// Temperature

void tempInit() {
  sensors.begin();
  
  #ifdef SERIAL_PRINT
    Serial.print(F("Parasite power: "));
    if (sensors.isParasitePowerMode()) {
      Serial.println(F("ON"));
    } else {
        Serial.println(F("OFF"));
      }
  #endif
  
  oneWire.reset_search();

  #ifdef SERIAL_PRINT
    // locate devices on the bus
    Serial.print(F("Found "));
    Serial.print(sensors.getDeviceCount());
    Serial.println(F(" devices"));
  #endif
  for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
    sensors.getAddress(tempSensor[i], i);
  }
  oneWire.reset_search();
  #ifdef SERIAL_PRINT
    showSensorsId();
  #endif
  checkTemp();
  #ifdef SERIAL_PRINT
    Serial.print(F("Temp1: "));
    Serial.println(temp1);
  #endif
}

#ifdef SERIAL_PRINT
  void showSensorsId() {
    for (byte n = 0; n < MAX_TEMP_SENSORS; n++) {
      Serial.print(F("#"));
      Serial.print(n);
      Serial.print(F(":"));
      for (byte i = 0; i < MAX_ID_DIGITS; i++) {
        Serial.print(F(" "));
        Serial.print(tempSensor[n][i]);
      }
      Serial.print(F(" "));
      Serial.println(objSens[n]);
    }
  }
#endif

void checkTemp() {
  #ifdef PROFILING
    unsigned long startTemp = millis();
  #endif
  sensors.requestTemperatures();
  for (byte n = 0; n < MAX_TEMP_SENSORS; n++) {
    switch (n) {
      case 0: 
        temp1 = getTemperature(0);
      #ifdef NRF24_FEATURE
        if      (temp1 == TEMP_NOT_PRESENT) {temp1NeedSend = NEED_SEND_NOT_PRESENT;}
        else if (temp1 == TEMP_NOT_DEFINED) {temp1NeedSend = NO_NEED_SEND;}        
        else if (temp1 == TEMP_REMOVED)     {temp1NeedSend = NEED_SEND_REMOVED;}
        else if (temp1 == TEMP_ERROR)       {temp1NeedSend = NEED_SEND_ERROR;}        
        else {
          if (temp1Back != TEMP_NOT_DEFINED) {
            float t1 = temp1 - temp1Back;
            if (abs(t1)           > 0.5) {temp1NeedSend = NEED_SEND_TEMP;}
            if (temp1 - temp1Back > 3)   {temp1NeedSend = NEED_SEND_ALARM_UP;}
            if (temp1Back - temp1 > 3)   {temp1NeedSend = NEED_SEND_ALARM_DOWN;}
          }
          temp1Back = temp1;
        } // else
      #endif // NRF24_FEATURE
        break;
      //case 1:
          //temp2 = getTemperature(1);
          break;
      //case 2:
          //temp3 = getTemperature(2);
          break;
      default: ;
    }
  } 
  #ifdef PROFILING
    unsigned long endTemp = millis() - startTemp;
    Serial.print(F("Temp prof: "));
    Serial.println(endTemp);
  #endif
} // checkTemp()

void tempWorks() {
  if (timer_30s) {
    checkTemp();
  }
  #ifdef NRF24_FEATURE
    if (temp1NeedSend) {
      sendPacket(DTA_TEMP_1);
    }
  #endif
}

#ifdef SERIAL_PRINT
  void printTempMess(char obj[], String mess) {
    Serial.print(obj);
    Serial.print(F(": "));
    Serial.println(mess);
  }
#endif

float getTemperature(byte n) {
  if (tempSensor[n][1] == idTempDat[n][1]
 //&& tempSensor[n][2] == idTempDat[n][2] 
 //&& tempSensor[n][3] == idTempDat[n][3]
     ) {
    currentTemp[n] = sensors.getTempC(tempSensor[n]);

    if (currentTemp[n] != -127) {
      if (currentTemp[n] != 85) {
        #ifdef SERIAL_PRINT
          //Serial.print(objSens[n]);
          //Serial.print(F(": "));
          //Serial.println(currentTemp[n]);
        #endif
        return currentTemp[n];
      } else {
          #ifdef SERIAL_PRINT
            printTempMess(objSens[n], F("error"));
          #endif
          return TEMP_ERROR; // 85
        }  
    } else {
        #ifdef SERIAL_PRINT
          printTempMess(objSens[n], F("removed"));
        #endif
        return TEMP_REMOVED; // -127
      }
  } else { // Sensor ID != stored Sensor ID
      #ifdef SERIAL_PRINT
        printTempMess(objSens[n], F("id not present"));
      #endif
      return TEMP_NOT_PRESENT; // -98
    }
} // getTemperature( )

#endif // TEMP_FEATURE
