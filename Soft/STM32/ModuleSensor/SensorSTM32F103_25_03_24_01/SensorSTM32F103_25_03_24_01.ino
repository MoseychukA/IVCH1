//----------------------------------------------------------------------------------------------------------------
/*
�������� ��� �������������� ������, ������������� ��� �����������
������ ���� �������������� �������� � �������� � ��� ��������� �� ���� 1-Wire.

����� �������������� ������ �� �����������, ��������� ������ LoRa,
��� ���� ����������� ���������������� USE_LORA.

*/
//----------------------------------------------------------------------------------------------------------------

#include <OneWire.h>
#include "UniGlobals.h"
#include "LowLevel.h"
#include "OneWireSlave.h"

//----------------------------------------------------------------------------------------------------------------
/*
 ����, ������� ���������� ����� ������ � ���������:
 
 */

//----------------------------------------------------------------------------------------------------------------
// ���������������� ���������
//----------------------------------------------------------------------------------------------------------------
#define SOIL_MOISTURE_0_PERCENT    530 // ������� ��� 0% ��������� �����, ��������� ������ ��������� (0-1023)
#define SOIL_MOISTURE_100_PERCENT  200 // ������� ��� 100% ��������� �����, ��������� ������ ��������� (0-1023)
//----------------------------------------------------------------------------------------------------------------
#define _DEBUG // ����������������� ��� ����������� ������ (������� � Serial, �� ������������ � ������������ RS-485 !!!)

//----------------------------------------------------------------------------------------------------------------
// ��������� LoRa
//----------------------------------------------------------------------------------------------------------------
#define USE_LORA                        // ����������������, ���� �� ���� �������� ����� LoRa.
/*
 LoRa ��� ����� ������ �������� ��������� ����: 9,10,11,12,13.
 ������� �� ���, ����� ������ ����� �� ������������ � ������, ��� � RS-485, ��� ��� ���.
 */
#define LORA_SS_PIN             PA4    // ��� SS ��� LoRa
#define LORA_RESET_PIN          PB0    // ��� Reset ��� LoRa
#define LORA_POWER_PIN          PB13    // ��� IO0 ��� LoRa
#define LORA_FREQUENCY        868E6    // ������� ������ (433E6, 868E6, 915E6)
#define LORA_TX_POWER            17    // �������� ����������� (1 - 17)

 //----------------------------------------------------------------------------------------------------------------
//
//#define PWM_PIN                8;    // ����� ����, �� ������� ����� ���������
//unsigned long frequency = 2000000;    // �������  1 - 2000000 (��)
//int brightness = 150;    // ������� ��� (0-255)  


//----------------------------------------------------------------------------------------------------------------
// ��������� �������� ��� ������, ������ �����!
//----------------------------------------------------------------------------------------------------------------
const SensorSettings Sensors[3] = {
       
 {mstChinaSoilMoistureMeter,33,0},     // PC14 ������ ��������� ����� �� ���� 
 {mstDS18B20,26,0},                  // PB10
 {mstNone,0, 0}


/* 
 
 �������������� ���� ��������: 
 
 
  {mstDS18B20,A0,0} - ������ DS18B20 �� ���� A0
  {mstChinaSoilMoistureMeter,A7,0} - ��������� ������ ��������� ����� �� ���� A7
 
  // ��������� ������� ��������� ����� ������ �� ������ �������� ���, �� ���������� �������� �������������� ��������� ����� !!! ������������ ����������� ���������� - 254, ����������� - 1.
  {mstFrequencySoilMoistureMeter,A5, 0} - ��������� ������ ��������� ����� �� ���������� ���� A5
  {mstFrequencySoilMoistureMeter,A4, 0} - ��������� ������ ��������� ����� �� ���������� ���� A4
  {mstFrequencySoilMoistureMeter,A3, 0} - ��������� ������ ��������� ����� �� ���������� ���� A3
  

  ���� � ����� ��������
    {mstNone,0, 0}
  �� ��� ������, ��� ������� �� ���� ����� ���   

 */

};
//----------------------------------------------------------------------------------------------------------------
// ������ ������ - ��������������� :)
//----------------------------------------------------------------------------------------------------------------
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// ||
// \/
//----------------------------------------------------------------------------------------------------------------
// ����� ���� ��������� ��������� � ��� - ������ � ������ ���������� ����, ��� ����� ������� !!!
//----------------------------------------------------------------------------------------------------------------
// ��������� 1-Wire
Pin oneWireData(PB12); // PB11 �� ������ ���� � ��� ����� 1-Wire
const byte owROM[7] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }; // ����� �������, ������ �� �����������, �.�. � ��� �� ������� 1-Wire
// ������� 1-Wire
const byte COMMAND_START_CONVERSION = 0x44; // ��������� �����������
const byte COMMAND_READ_SCRATCHPAD  = 0xBE; // ��������� ������ ��������� �������
const byte COMMAND_WRITE_SCRATCHPAD = 0x4E; // ��������� �������� ���������, ������ ����� ���������
const byte COMMAND_SAVE_SCRATCHPAD  = 0x25; // ��������� ��������� ��������� � EEPROM
enum DeviceState {
  DS_WaitingReset,
  DS_WaitingCommand,
  DS_ReadingScratchpad,
  DS_SendingScratchpad
};
volatile DeviceState state                 = DS_WaitingReset;  
volatile byte scratchpadWritePtr           = 0; // ��������� �� ���� � ����������, ���� ���� �������� ��������� �� ������� ����
volatile byte scratchpadNumOfBytesReceived = 0; // ������� ���� ��������� �� �������

                                                //----------------------------------------------------------------------------------------------------------------
#define ROM_ADDRESS (void*) 123 // �� ������ ������ � ��� ���������?
//----------------------------------------------------------------------------------------------------------------
t_scratchpad scratchpadS, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS;          //��� �� ���������� � scratchpad ��� � ��������� �������

volatile bool scratchpadReceivedFromMaster     = false;    // ����, ��� �� �������� ������ � �������
volatile bool needToMeasure                    = false;    // ����, ��� �� ������ ��������� �����������
volatile unsigned long sensorsUpdateTimer      = 0;        // ������ ��������� ���������� � �������� � ���������� ������ � ����������
volatile bool measureTimerEnabled              = false;    // ����, ��� �� ������ ��������� ������ � �������� ����� ������ ���������
volatile unsigned long query_interval          = MEASURE_MIN_TIME; // ��� ����� �������� ������
unsigned long last_measure_at                  = 0;        // ����� � ��������� ��� ��������� �����������

volatile bool connectedViaOneWire              = false;    // ����, ��� �� ������������ � ����� 1-Wire, ��� ���� �� �� ����� � ���� �� nRF � �� ��������� ��������� �� RS-485
volatile bool needResetOneWireLastCommandTimer = false;
volatile unsigned long oneWireLastCommandTimer = 0;

//-------------------------------------------------------------------------------------------------------------------------------------------------------
byte GetSensorType(const SensorSettings& sett)
{
  switch(sett.Type)
  {
    case mstNone:
      return uniNone;
    
    case mstDS18B20:
      return uniTemp;

    case mstChinaSoilMoistureMeter:
    case mstFrequencySoilMoistureMeter:
      return uniSoilMoisture;
    
  }

  return uniNone;
}
//----------------------------------------------------------------------------------------------------------------
void SetDefaultValue(const SensorSettings& sett, byte* data)
{
  switch(sett.Type)
  {
    case mstNone:
      *data = 0xFF;
    break;
    
    case mstDS18B20:
    case mstChinaSoilMoistureMeter:
    case mstFrequencySoilMoistureMeter:
    {
      *data = NO_TEMPERATURE_DATA;
    }
    break;
  }
}
//----------------------------------------------------------------------------------------------------------------
void* SensorDefinedData[3] = {NULL}; // ������, ����������� ��������� ��� �������������
//----------------------------------------------------------------------------------------------------------------
void* InitSensor(const SensorSettings& sett)
{
  switch(sett.Type)
  {
    case mstNone:
      return NULL;
    
    case mstDS18B20:
      return InitDS18B20(sett);

    case mstFrequencySoilMoistureMeter:
        return InitFrequencySoilMoistureMeter(sett);
    break;
    case mstChinaSoilMoistureMeter:
      return NULL;
    break;
  }

  return NULL;  
}
//----------------------------------------------------------------------------------------------------------------
void ReadROM()
{
    memset((void*)&scratchpadS,0,sizeof(scratchpadS));
   // eeprom_read_block((void*)&scratchpadS, ROM_ADDRESS, 29);

    // ����� ����� ������ �� ���������
  /*  if(scratchpadS.rf_id == 0xFF || scratchpadS.rf_id == 0)
      scratchpadS.rf_id = DEFAULT_RF_CHANNEL; */

    scratchpadS.packet_type = ptSensorsData; // �������, ��� ��� ��� ������ - ������ � ���������
    scratchpadS.packet_subtype = 0;


    // ���� ��������� ������ �� ��������� - ���������� �� ���������
    if(scratchpadS.query_interval_min == 0xFF)
      scratchpadS.query_interval_min = 0;
      
    if(scratchpadS.query_interval_sec == 0xFF)
      scratchpadS.query_interval_sec =  MEASURE_MIN_TIME/1000;

   if(scratchpadS.query_interval_min == 0 && scratchpadS.query_interval_sec < 5) // ������� 5 ������ ����� ������������ ��������
    scratchpadS.query_interval_sec = 5;

    // ��������� �������� ������
    query_interval = (scratchpadS.query_interval_min*60 + scratchpadS.query_interval_sec)*1000;

    #ifdef _DEBUG
      Serial.print(F("Query interval: "));
      Serial.println(query_interval);
    #endif
    
    scratchpadS.sensor1.type = GetSensorType(Sensors[0]);
    scratchpadS.sensor2.type = GetSensorType(Sensors[1]);
    scratchpadS.sensor3.type = GetSensorType(Sensors[2]);

    SetDefaultValue(Sensors[0],scratchpadS.sensor1.data);
    SetDefaultValue(Sensors[1],scratchpadS.sensor2.data);
    SetDefaultValue(Sensors[2],scratchpadS.sensor3.data);

    // �������, ���� �� � ��� ����������?
    byte calibration_enabled = false;
    for(byte i=0;i<3;i++)
    {
        switch(Sensors[i].Type)
        {
            case mstChinaSoilMoistureMeter:
            {
              calibration_enabled = true;
              // ������������� �������� �� ���������
              if(scratchpadS.calibration_factor1 == 0xFF && scratchpadS.calibration_factor2 == 0xFF)
              {
                // � EEPROM ������ ��� �� ����� ������, ��� �������� ���������
                scratchpadS.calibration_factor1 = map(SOIL_MOISTURE_0_PERCENT,0,1023,0,255);
                scratchpadS.calibration_factor2 = map(SOIL_MOISTURE_100_PERCENT,0,1023,0,255);
              }

              // �� ������������ ��� ������� ����������
              scratchpadS.config |= (4 | 8);
            }
            break; // mstChinaSoilMoistureMeter

        } // switch

        if(calibration_enabled)
          break;
    
    } // for

    if(calibration_enabled)
    {
      // ������� ������ ����������
      scratchpadS.config |= 2; // ������������� ������ ���, ������, ��� �� ������������ ����������
    } // if
    else
    {
      scratchpadS.config &= ~2; // ������ ��� ������� ��-������
    }

}
//----------------------------------------------------------------------------------------------------------------
void WakeUpSensor(const SensorSettings& sett, void* sensorDefinedData)
{  
  // ��������� �������
  switch(sett.Type)
  {
    case mstNone:
      break;
    
    case mstDS18B20:
      InitDS18B20(sett);
    break;

    case mstChinaSoilMoistureMeter:
    break;

    case mstFrequencySoilMoistureMeter:
    {
      Pin pin(sett.Pin);
      pin.inputMode();
      pin.writeHigh();      
    }
    break;
  }    
}

//----------------------------------------------------------------------------------------------------------------
void* InitFrequencySoilMoistureMeter(const SensorSettings& sett)
{
    UNUSED(sett);
    return NULL;  
}

//----------------------------------------------------------------------------------------------------------------
void* InitDS18B20(const SensorSettings& sett) // �������������� ������ �����������
{
  #ifdef _DEBUG
    Serial.println(F("Init DS18B20..."));
  #endif
  
  if(!sett.Pin) {
    #ifdef _DEBUG
      Serial.println(F("DS18B20 - no pin number!!!"));
    #endif
    return NULL; 
  }  

   OneWire ow(sett.Pin);

  if(!ow.reset()) // ��� �������
  {
     #ifdef _DEBUG
      Serial.println(F("DS18B20 - not found during init!!!"));
    #endif
    return NULL;  
  }

   ow.write(0xCC); // ����� �� ������ (SKIP ROM)
   ow.write(0x4E); // ��������� ������ � scratchpad

   ow.write(0); // ������� ������������� ����� 
   ow.write(0); // ������ ������������� �����
   ow.write(0x7F); // ���������� ������� 12 ���

   ow.reset();
   ow.write(0xCC); // ����� �� ������ (SKIP ROM)
   ow.write(0x48); // COPY SCRATCHPAD
   delay(10);
   ow.reset();

  #ifdef _DEBUG
    Serial.println(F("DS18B20 - inited."));
  #endif

   return NULL;
    
}
//----------------------------------------------------------------------------------------------------------------
void InitSensors()
{
  #ifdef _DEBUG
    Serial.println(F("Init sensors..."));
  #endif
  
  // �������������� �������
  for(byte i=0;i<3;i++)
    SensorDefinedData[i] = InitSensor(Sensors[i]);
         
}
//----------------------------------------------------------------------------------------------------------------
 void ReadDS18B20(const SensorSettings& sett, struct sensor* s) // ������ ������ � ������� �����������
{ 
  
  #ifdef _DEBUG
    Serial.println(F("Read DS18B20..."));
  #endif
  
  s->data[0] = NO_TEMPERATURE_DATA;
  s->data[1] = 0;
  
  if(!sett.Pin)
  {
    #ifdef _DEBUG
      Serial.println(F("DS18B20 - no pin number!!!"));
    #endif       
    return;
  }

   OneWire ow(sett.Pin);
    
    if(!ow.reset()) // ��� ������� �� �����
    {
    #ifdef _DEBUG
      Serial.println(F("DS18B20 - not found!"));
    #endif   
    return; 
    }

  byte data[9] = {0};
  
  ow.write(0xCC); // ����� �� ������ (SKIP ROM)
  ow.write(0xBE); // ������ scratchpad ������� �� ����

  for(uint8_t i=0;i<9;i++)
    data[i] = ow.read();


 if (OneWire::crc8(data, 8) != data[8]) // ��������� ����������� �����
 {
  #ifdef _DEBUG
    Serial.println(F("DS18B20 - bad checksum!!!"));
  #endif     
      return;
 }
  int loByte = data[0];
  int hiByte = data[1];

  int temp = (hiByte << 8) + loByte;
  
  bool isNegative = (temp & 0x8000);
  
  if(isNegative)
    temp = (temp ^ 0xFFFF) + 1;

  int tc_100 = (6 * temp) + temp/4;

  if(isNegative)
    tc_100 = -tc_100;
   
  s->data[0] = tc_100/100;
  s->data[1] = abs(tc_100 % 100);

  #ifdef _DEBUG
    Serial.print(F("DS18B20: "));
    Serial.print(s->data[0]);
    Serial.print(F(","));
    Serial.println(s->data[1]);
    
  #endif     
    
}
//----------------------------------------------------------------------------------------------------------------
void ReadFrequencySoilMoistureMeter(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
    UNUSED(sensorDefinedData);

 int highTime = pulseIn(sett.Pin,HIGH);
 
 if(!highTime) // always HIGH ?
 {
   s->data[0] = NO_TEMPERATURE_DATA;

  // Serial.println("ALWAYS HIGH,  BUS ERROR!");

   return;
 }
 highTime = pulseIn(sett.Pin,HIGH);
 int lowTime = pulseIn(sett.Pin,LOW);


 if(!lowTime)
 {
  return;
 }
  int totalTime = lowTime + highTime;
  // ������ ������� ��������� highTime � ����� ����� ��������� - ��� � ����� ��������� �����
  // totalTime = 100%
  // highTime = x%
  // x = (highTime*100)/totalTime;

  float moisture = (highTime*100.0)/totalTime;

  int moistureInt = moisture*100;

   s->data[0] = moistureInt/100;
   s->data[1] = moistureInt%100;
 
}
//----------------------------------------------------------------------------------------------------------------
void ReadChinaSoilMoistureMeter(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
   UNUSED(sensorDefinedData);
   
   int val = analogRead(sett.Pin);

#ifdef _DEBUG
   Serial.print(F("Val: "));
   Serial.println(val); 
 /*  Serial.print(s->data[0]);
   Serial.print(F(","));
   Serial.println(s->data[1]);*/

#endif  
   
   int soilMoisture0Percent = map(scratchpadS.calibration_factor1,0,255,0,1023);
   int soilMoisture100Percent = map(scratchpadS.calibration_factor2,0,255,0,1023);

   int percentsInterval = map(val,min(soilMoisture0Percent,soilMoisture100Percent),max(soilMoisture0Percent,soilMoisture100Percent),0,10000);
   
  // ������, ���� � ��� �������� 0% ��������� ������, ��� �������� 100% ��������� - ���� �� 10000 ������ ���������� ��������
  if(soilMoisture0Percent > soilMoisture100Percent)
    percentsInterval = 10000 - percentsInterval;

   int8_t sensorValue;
   byte sensorFract;

   sensorValue = percentsInterval/100;
   sensorFract = percentsInterval%100;

   if(sensorValue > 99)
   {
      sensorValue = 100;
      sensorFract = 0;
   }

   if(sensorValue < 0)
   {
      sensorValue = NO_TEMPERATURE_DATA;
      sensorFract = 0;
   }

   s->data[0] = sensorValue;
   s->data[1] = sensorFract;

#ifdef _DEBUG
   Serial.print(F("data: "));
   Serial.println(s->data[0]);

#endif  

   
}
//----------------------------------------------------------------------------------------------------------------
void ReadSensor(const SensorSettings& sett, void* sensorDefinedData, struct sensor* s)
{
  switch(sett.Type)
  {
    case mstNone:
      
    break;

    case mstDS18B20:
    ReadDS18B20(sett,s);
    break;

    case mstChinaSoilMoistureMeter:
      ReadChinaSoilMoistureMeter(sett,sensorDefinedData,s);
    break;

    case mstFrequencySoilMoistureMeter:
      ReadFrequencySoilMoistureMeter(sett,sensorDefinedData,s);
    break;
  }
}
//----------------------------------------------------------------------------------------------------------------
void ReadSensors()
{
  #ifdef _DEBUG
    Serial.println(F("Read sensors..."));
  #endif  
  // ������ ���������� � ��������
    
  ReadSensor(Sensors[0],SensorDefinedData[0],&scratchpadS.sensor1);
  ReadSensor(Sensors[1],SensorDefinedData[1],&scratchpadS.sensor2);
  ReadSensor(Sensors[2],SensorDefinedData[2],&scratchpadS.sensor3);

}
//----------------------------------------------------------------------------------------------------------------
void MeasureDS18B20(const SensorSettings& sett)
{
    
  #ifdef _DEBUG
    Serial.println(F("DS18B20 - start conversion..."));
  #endif
  
  if(!sett.Pin)
  {
    #ifdef _DEBUG
      Serial.println(F("DS18B20 - no pin number!!!"));
    #endif
    return;
  }

   OneWire ow(sett.Pin);
    
    if(!ow.reset()) // ��� ������� �� �����
    {
      #ifdef _DEBUG
        Serial.println(F("DS18B20 - not found!!!"));
      #endif    
      return; 
    }

    ow.write(0xCC);
    ow.write(0x44); // �������� ������� �� ����� ���������
    
    ow.reset();    

  #ifdef _DEBUG
    Serial.println(F("DS18B20 - converted."));
  #endif    
  
}
//----------------------------------------------------------------------------------------------------------------
bool HasI2CSensors()
{
  // ���������, ���� �� � ��� ���� ���� ������ �� I2C
  for(byte i=0;i<3;i++)
  {
    switch(Sensors[i].Type)
    {
      case mstBH1750:
      case mstSi7021:
      case mstMAX44009:
        return true;
    }
    
  } // for
  return false;
}


//----------------------------------------------------------------------------------------------------------------
void MeasureSensor(const SensorSettings& sett,void* sensorDefinedData) // ��������� ����������� � �������, ���� ����
{
  switch(sett.Type)
  {
    case mstNone:    
    break;

    case mstDS18B20:
    MeasureDS18B20(sett);
    break;

    case mstChinaSoilMoistureMeter:
    case mstFrequencySoilMoistureMeter:
    break;
  }  
}

//----------------------------------------------------------------------------------------------------------------
void UpdateSensor(const SensorSettings& sett,void* sensorDefinedData, unsigned long curMillis)
{
  // ��������� ������� �����
  switch(sett.Type)
  {

    case mstNone:    
    case mstDS18B20:
    case mstChinaSoilMoistureMeter:
    case mstFrequencySoilMoistureMeter:
    break;
  }  
}
//----------------------------------------------------------------------------------------------------------------
void UpdateSensors()
{
  unsigned long thisMillis = millis();
  for(byte i=0;i<3;i++)
    UpdateSensor(Sensors[i],SensorDefinedData[i],thisMillis);  
}
//----------------------------------------------------------------------------------------------------------------
void StartMeasure()
{  
  #ifdef _DEBUG
    Serial.println(F("Start measure..."));
  #endif
 
  // ��������� �����������
  for(byte i=0;i<3;i++)
    MeasureSensor(Sensors[i],SensorDefinedData[i]);

  last_measure_at = millis();
}
//----------------------------------------------------------------------------------------------------------------
#ifdef USE_LORA
//----------------------------------------------------------------------------------------------------------------
#include "LoRa.h"
bool loRaInited = false;
//----------------------------------------------------------------------------------------------------------------
void initLoRa()
{
  #ifdef _DEBUG
  Serial.begin(57600);
  #endif
  
  // �������������� LoRa
  LoRa.setPins(LORA_SS_PIN,LORA_RESET_PIN,-1);
  loRaInited = LoRa.begin(LORA_FREQUENCY);

  if(loRaInited)
  {
    LoRa.setTxPower(LORA_TX_POWER);
    //LoRa.receive(); // �������� �������
    LoRa.sleep(); // ��������
  } // nRFInited
  
}
//----------------------------------------------------------------------------------------------------------------
void sendDataViaLoRa()
{
  if(!loRaInited) {
 #ifdef _DEBUG
  Serial.println(F("LoRa not inited!"));
 #endif    
    return;
  }
    
  if(!((scratchpadS.config & 1) == 1))
  {
    #ifdef _DEBUG
    Serial.println(F("LoRa: transiever disabled."));
    #endif
    return;
  }
    
  #ifdef _DEBUG
    Serial.println(F("Send sensors data via LoRa..."));
  #endif

  bool sendDone = false;

    for(int i=0;i<5;i++) // �������� ������� 5 ���
    {
        // ������������ ����������� �����
        scratchpadS.crc8 = OneWireSlave::crc8((const byte*)&scratchpadS,sizeof(scratchpadS)-1);  
        LoRa.beginPacket();
        LoRa.write((byte*)&scratchpadS,sizeof(scratchpadS)); // ����� � ����
        if(LoRa.endPacket()) // ����� � ����
        {
          sendDone = true;
          break;
        }
        else
        {
          delay(random(10));
        }
    } // for

    if(!sendDone)
    {
      #ifdef _DEBUG
        Serial.println(F("NO RECEIVING SIDE FOUND!"));
      #endif      
    }
    else
    {
      #ifdef _DEBUG
        Serial.println(F("Sensors data sent."));
      #endif
    }
    
  //LoRa.receive();

  // ��������� ��������
  delay(random(50));

  LoRa.sleep(); // ��������

}
//----------------------------------------------------------------------------------------------------------------
#endif // USE_LORA
//----------------------------------------------------------------------------------------------------------------
void WriteROM()
{

    scratchpadS.sensor1.type = GetSensorType(Sensors[0]);
    scratchpadS.sensor2.type = GetSensorType(Sensors[1]);
    scratchpadS.sensor3.type = GetSensorType(Sensors[2]);
  
   // eeprom_write_block( (void*)scratchpad,ROM_ADDRESS,29);
    memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));
    scratchpadToSend.crc8 = OneWireSlave::crc8((const byte*)&scratchpadToSend,sizeof(scratchpadS)-1);

    #ifdef USE_NRF
      // ������������� ����� �����
      if(nRFInited)
        radio.setChannel(scratchpadS.rf_id);
        
    #endif
    

}
//----------------------------------------------------------------------------------------------------------------
void owReceive(OneWireSlave::ReceiveEvent evt, byte data);
//----------------------------------------------------------------------------------------------------------------
void setup()
{
  #ifdef _DEBUG
    Serial.begin(57600);
  #endif
  
    
  ReadROM();
  
  scratchpadS.crc8 = OneWireSlave::crc8((const byte*) scratchpad,sizeof(scratchpadS)-1);
  memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));

   InitSensors(); // �������������� �������   

    #ifdef USE_LORA
      initLoRa();
    #endif

  oneWireLastCommandTimer = millis();
  
  OWSlave.setReceiveCallback(&owReceive);
  OWSlave.begin(owROM, oneWireData.getPinNumber());


  // InitTimersSafe();                                  //�������������� ��� �������, ����� 0,
  ////Timer2_Initialize();
  //SetPinFrequencySafe(PWM_PIN, frequency);           //������������� ������� ��� ���������� pin
  //SetPinFrequency(PWM_PIN, frequency);               //������������� ������� ��� ���������� pin
  //pwmWrite(PWM_PIN, brightness);                     //0-255   ����������� ��� ������� ������ analogWrite 

  
}
//----------------------------------------------------------------------------------------------------------------
void owSendDone(bool error) {
  UNUSED(error);
 // ��������� �������� ��������� �������
 state = DS_WaitingReset;
}
//----------------------------------------------------------------------------------------------------------------
// ���������� ���������� �� ����
void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
  connectedViaOneWire = true; // �������, ��� �� ���������� ����� 1-Wire
  needResetOneWireLastCommandTimer = true; // ������, ����� �������� ������ ������� ��������� ��������� �������
  
  switch (evt)
  {
  case OneWireSlave::RE_Byte:
    switch (state)
    {

     case DS_ReadingScratchpad: // ������ ��������� �� �������

        // ����������� ���-�� ����������� ����
        scratchpadNumOfBytesReceived++;

        // ����� � ��������� �������� ����
        scratchpad[scratchpadWritePtr] = data;
        // ����������� ��������� ������
        scratchpadWritePtr++;

        // ���������, �� �� ���������
        if(scratchpadNumOfBytesReceived >= sizeof(scratchpadS)) {
          // �� ���������, ���������� ��������� �� �������� ������
          state = DS_WaitingReset;
          scratchpadNumOfBytesReceived = 0;
          scratchpadWritePtr = 0;
          scratchpadReceivedFromMaster = true; // �������, ��� �� �������� ��������� �� �������
          // ��������� ����� �������� ������
          //query_interval = (scratchpadS.query_interval_min*60 + scratchpadS.query_interval_sec)*1000;
        }
        
     break; // DS_ReadingScratchpad
      
    case DS_WaitingCommand:
      switch (data)
      {
      case COMMAND_START_CONVERSION: // ��������� �����������
        state = DS_WaitingReset;
        if(!measureTimerEnabled && !needToMeasure) // ������ ���� ��� ��� �� ��������
          needToMeasure = true;
        break;

      case COMMAND_READ_SCRATCHPAD: // ��������� ������ ��������� �������
        state = DS_SendingScratchpad;
        OWSlave.beginWrite((const byte*)&scratchpadToSend, sizeof(scratchpadToSend), owSendDone);
        break;

      case COMMAND_WRITE_SCRATCHPAD:  // ��������� �������� ���������, ������ ����� ���������
          state = DS_ReadingScratchpad; // ��� ����������
          scratchpadWritePtr = 0;
          scratchpadNumOfBytesReceived = 0;
        break;

        case COMMAND_SAVE_SCRATCHPAD: // ��������� ��������� � ������
          state = DS_WaitingReset;
          WriteROM();
        break;

        default:
          state = DS_WaitingReset;
        break;
        

      } // switch (data)
      break; // case DS_WaitingCommand

      case DS_WaitingReset:
      break;

      case DS_SendingScratchpad:
      break;
    } // switch(state)
    break; 

  case OneWireSlave::RE_Reset:
    state = DS_WaitingCommand;
    break;

  case OneWireSlave::RE_Error:
    state = DS_WaitingReset;
    break;
    
  } // switch (evt)
}
//----------------------------------------------------------------------------------------------------------------
void loop()
{

  if(scratchpadReceivedFromMaster) 
  {
    scratchpadReceivedFromMaster = false;

    
    // ������ ��� ������� �� �������, ��� ����� ���-�� ������
    memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));
    scratchpadToSend.crc8 = OneWireSlave::crc8((const byte*) &scratchpadToSend,sizeof(scratchpadS)-1);

    // ��������� ����� �������� ������
    query_interval = (scratchpadToSend.query_interval_min*60 + scratchpadToSend.query_interval_sec)*1000;
          
     #ifdef _DEBUG
        Serial.println(F("Scratch received from master!"));
     #endif
      
  } // scratchpadReceivedFromMaster

  
  unsigned long curMillis = millis();

  // ���� ��������� �������� ������ ��������� ��������� ������� �� ����� 1-Wire - ������ ���
  if(needResetOneWireLastCommandTimer) 
  {
    oneWireLastCommandTimer = curMillis;
    needResetOneWireLastCommandTimer = false;
  }

  // ��������� - ����� ��������� ��������� ������� �� 1-Wire: ���� � �� ���� ������ 15 ������ - ���������� nRF � RS-485
  if(connectedViaOneWire) 
  {
      if((curMillis - oneWireLastCommandTimer) > 15000) 
      {
         #ifdef _DEBUG
            Serial.print(F("Last command at: "));
            Serial.print(oneWireLastCommandTimer);
            Serial.print(F("; curMillis: "));
            Serial.print(curMillis);
            Serial.print(F("; diff = "));
            Serial.println((curMillis - oneWireLastCommandTimer));
         #endif
        
          connectedViaOneWire = false; // ���������� ����� 1-Wire ���������
      }
  }
  


  if(!connectedViaOneWire && ((curMillis - last_measure_at) > query_interval) && !measureTimerEnabled && !needToMeasure) 
  {
    // ����-�� ����� �� ��������� �����������, ��������, �������
      // � �������� ������ �����, ����� �� �� ���������� � 1-Wire, ����� - ������ ��� �������� �����������.
        needToMeasure = true;
        #ifdef _DEBUG
          Serial.println(F("Want measure by timeout..."));
        #endif        
  }

  // ������ ���� ������ �� ������ �� ����� 1-Wire � ��������� �����������
  
  if(needToMeasure && !measureTimerEnabled) 
  {
    #ifdef _DEBUG
      Serial.println(F("Want measure..."));
    #endif    

    measureTimerEnabled = true; // �������� ����, ��� �� ������ ��������� ������ � ��������
    sensorsUpdateTimer = curMillis; // ���������� ������ ����������
    StartMeasure();

    needToMeasure = false;
    
    #ifdef _DEBUG
      Serial.println(F("Wait for measure complete..."));
    #endif    
  }
 

  if(measureTimerEnabled) 
  {
    UpdateSensors(); // ��������� �������, ���� ����-�� �� ��� ����� ������������� ����������
  }
  
  if(measureTimerEnabled && ((curMillis - sensorsUpdateTimer) > MEASURE_MIN_TIME)) 
  {
    
    if(state != DS_SendingScratchpad)
    {

          #ifdef _DEBUG
            Serial.println(F("Measure completed, start read..."));
          #endif
        
             // ����� ������ ���������� � ��������
             ReadSensors();
             
             //noInterrupts();
             memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));
             scratchpadToSend.crc8 = OneWireSlave::crc8((const byte*) &scratchpadToSend,sizeof(scratchpadS)-1);
             //interrupts();
        

          #ifdef _DEBUG
            Serial.println(F("Sensors data readed."));
          #endif     
  
             #ifdef USE_LORA
              if(!connectedViaOneWire)
                sendDataViaLoRa();
             #endif             
        
          #ifdef _DEBUG
            Serial.println(F(""));
          #endif 
          
             sensorsUpdateTimer = curMillis;
             measureTimerEnabled = false;
             
      } // if(state != DS_SendingScratchpad)      
  }

 

}
//----------------------------------------------------------------------------------------------------------------
void yield()
{
 
}
//----------------------------------------------------------------------------------------------------------------
