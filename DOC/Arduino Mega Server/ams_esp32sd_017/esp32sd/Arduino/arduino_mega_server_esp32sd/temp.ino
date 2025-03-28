/*
  Module Temperature
  part of Arduino Mega Server project
*/

#ifdef FEATURE_TEMP

#include <DallasTemperature.h>
#include <OneWire.h>

#define ONE_WIRE_BUS      2
#define MAX_ID_DIGITS     7
#define FULL_HEALTH       100
#define ZERO_HEALTH       0
#define MAX_TEMP_NAME_LEN 12
#define MAX_TEMP_SENSORS  3
byte tempSensorHealth[MAX_TEMP_SENSORS];
byte tempModuleHealth;

char objSens[MAX_TEMP_SENSORS][MAX_TEMP_NAME_LEN] = {"tempSTR", "tempHOM", "tempWRM"};

// Sensors ID's
byte idTempDat[MAX_TEMP_SENSORS][MAX_ID_DIGITS] = { 
  {40, 240, 72, 81, 3, 0, 0},
  {40, 221, 16, 81, 3, 0, 0},
  {40,  23, 98, 87, 3, 0, 0},
}; 

float current_temp[MAX_TEMP_SENSORS];
int temp_[MAX_TEMP_SENSORS];

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempSensor[MAX_TEMP_SENSORS];

void initTemp() {
  initStart("Temperature");
  sensors.begin();
  timeStamp();
  Serial.print("Parasite power ");
  if (sensors.isParasitePowerMode()) {Serial.println(F("ON"));}
                                else {Serial.println(F("OFF"));}
  oneWire.reset_search();

  // locate devices on the bus
  timeStamp();
  Serial.print(F("Found "));
  Serial.print(sensors.getDeviceCount());
  Serial.println(F(" devices"));

  for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
    sensors.getAddress(tempSensor[i], i);
    tempSensorHealth[i] = FULL_HEALTH;
  }
  tempModuleHealth = FULL_HEALTH;
  oneWire.reset_search();
  
  showSensorsId();
  moduleTemp = MODULE_ENABLE;
  initDone();
}

void showSensorsId() {
  for (byte n = 0; n < MAX_TEMP_SENSORS; n++) {
    timeStamp();
    Serial.print(F("#"));
    Serial.print(n);
    Serial.print(F(":"));
    for (byte i = 0; i < MAX_ID_DIGITS; i++) {
      Serial.print(" ");
      Serial.print(tempSensor[n][i]);
    }
    Serial.print(" ");
    Serial.println(objSens[n]);
  }
}

void setModuleTempHealth() {
  int sumSensorsHealth = 0;
  for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
    sumSensorsHealth += tempSensorHealth[i];
  }
  tempModuleHealth = sumSensorsHealth / MAX_TEMP_SENSORS;
}

void workTemp() {
  if (cycle5m) {
    sensors.requestTemperatures();
    getTemperature(0); // STR
    getTemperature(1); // HOM (SRV)
    getTemperature(2); // WRM  
    #ifdef TEMP_DEBUG
      for (byte i = 0; i < MAX_TEMP_SENSORS; i++) {
        timeStamp();
        Serial.print(F("Sensor "));
        Serial.print(objSens[i]);
        Serial.print(F(" health: "));
        Serial.println(tempSensorHealth[i]);
      }
      setModuleTempHealth();
      timeStamp();
      Serial.print(F("Modile Temperature Health: "));
      Serial.println(tempModuleHealth);
    #endif // TEMP_DEBUG
  }
}
  
void getTemperature(byte n) {
  if (tempSensor[n][1] == idTempDat[n][1] && 
      tempSensor[n][2] == idTempDat[n][2] && 
      tempSensor[n][3] == idTempDat[n][3]) {
    tempSensorHealth[n] = FULL_HEALTH;
    current_temp[n] = sensors.getTempC(tempSensor[n]);

    if (current_temp[n] != -127) {
      if (current_temp[n] != 85) {
        timeStamp();
        Serial.print(objSens[n]);
        Serial.print(": ");
        Serial.println(current_temp[n]);
         
        temp_[n] = (current_temp[n] - (int)current_temp[n]) * 100;

        if ((current_temp[n] > -1) && (current_temp[n] < 0)) {
          #ifdef FEATURE_MAJOR
            sendRequestMinusM(objSens[n], "-", (int)current_temp[n], abs(temp_[n])); 
          #endif          
        } else {
            #ifdef FEATURE_MAJOR
              sendRequestMinusM(objSens[n], "", (int)current_temp[n], abs(temp_[n])); 
            #endif            
          }  

      } else { // current_temp[n] = 85
          tempSensorHealth[n] = ZERO_HEALTH;
          timeStamp();
          Serial.print(objSens[n]);
          Serial.print(": ");
          Serial.println(F("error")); 
        }  
    } else { // current_temp[n] = -127
        tempSensorHealth[n] = ZERO_HEALTH;
        timeStamp();
        Serial.print(objSens[n]);
        Serial.print(": ");
        Serial.println(F("removed")); 
      }
  } else { // current Sensor ID != stored Sensor ID
      tempSensorHealth[n] = ZERO_HEALTH;
      timeStamp();
      Serial.print(objSens[n]);
      Serial.print(": ");
      Serial.println(F("ID problem or not present"));
    }
} // getTemperature( )  

#endif // FEATURE_TEMP
