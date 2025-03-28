/*
  Module Temperature / DS18B20
  part of Arduino Mega Server project
*/

#ifdef DS18B20_FEATURE

#include <DallasTemperature.h>
#include <OneWire.h>

#define DS18B20_PIN         2
#define MAX_DS18B20_SENSORS 3
#define MAX_ID_DIGITS       7

char objSens[MAX_DS18B20_SENSORS][12] = {"temp1", "temp2", "temp2"};

// Sensors ID's
byte idTempDat[MAX_DS18B20_SENSORS][MAX_ID_DIGITS] = { 
  {40, 240, 72, 81, 3, 0, 0},
  {40, 221, 16, 81, 3, 0, 0},
  {40,  23, 98, 87, 3, 0, 0},
};

float current_temp[MAX_DS18B20_SENSORS];
int temp_[MAX_DS18B20_SENSORS];

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress tempSensor[MAX_DS18B20_SENSORS];

void tempInit() {
  initStart("Temperature");
  sensors.begin();
  timeStamp();
  Serial.print(F(" Parasite power "));
  if (sensors.isParasitePowerMode()) {Serial.println(F("ON"));}
                                else {Serial.println(F("OFF"));}
  oneWire.reset_search();

  // locate devices on bus
  timeStamp(); Serial.print(F(" Found ")); Serial.print(sensors.getDeviceCount()); Serial.println(F(" devices"));

  for (byte i = 0; i < MAX_DS18B20_SENSORS; i++) {
    sensors.getAddress(tempSensor[i], i);
  }
  oneWire.reset_search();
  
  showSensorsId();
  moduleDs18b20 = MODULE_ENABLE;
  initDone();
} // tempInit()

byte getMaxDs18b20Sensors() {
  return MAX_DS18B20_SENSORS;
}

String getDs18b20Name(byte i) {
  return objSens[i];
}

float getDs18b20Value(byte i) {
  return current_temp[i];
}

void showSensorsId() {
  for (byte n = 0; n < MAX_DS18B20_SENSORS; n++) {
    timeStamp();
    Serial.print(F(" #"));
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
 
void getTemperature(byte n) {
  if (tempSensor[n][1] == idTempDat[n][1] && 
      tempSensor[n][2] == idTempDat[n][2] && 
      tempSensor[n][3] == idTempDat[n][3]) {
    current_temp[n] = sensors.getTempC(tempSensor[n]);

    if (current_temp[n] != -127) {
      if (current_temp[n] != 85) {
        timeStamp();
        Serial.print(objSens[n]); Serial.print(F(": ")); Serial.println(current_temp[n]);
         
        temp_[n] = (current_temp[n] - (int)current_temp[n]) * 100;

        if ((current_temp[n] > -1) && (current_temp[n] < 0)) {
          #ifdef MAJORDOMO_FEATURE
            sendRequestMinusM(objSens[n], "-", (int)current_temp[n], abs(temp_[n])); 
          #endif          
        } else {
            #ifdef MAJORDOMO_FEATURE
              sendRequestMinusM(objSens[n], "", (int)current_temp[n], abs(temp_[n])); 
            #endif            
          }  

      } else { // current_temp[n] = 85
          timeStamp();
          Serial.print(objSens[n]); Serial.print(F(": ")); Serial.println(F("error")); 
        }  
    } else { // current_temp[n] = -127
        timeStamp();
        Serial.print(objSens[n]); Serial.print(F(": ")); Serial.println(F("removed")); 
      }
  } else { // current Sensor ID != stored Sensor ID
      timeStamp();
      Serial.print(objSens[n]); Serial.print(F(": ")); Serial.println(F("ID problem or not present"));
    }
} // getTemperature( )  

void tempWorks() {
  if (cycle5m) {
    sensors.requestTemperatures();
    getTemperature(0);
    getTemperature(1);
    getTemperature(2);
  }
}

#endif // DS18B20_FEATURE
