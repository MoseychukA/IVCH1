//----------------------------------------------------------------------------------------------------------------
/*
Прошивка для универсального модуля, предназначена для подключения
любого типа поддерживаемых датчиков и передачи с них показаний по шине 1-Wire.

Также поддерживается работа по радиоканалу, используя модуль LoRa,
для этой возможности раскомментируйте USE_LORA.

*/
//----------------------------------------------------------------------------------------------------------------

#include <OneWire.h>
#include "UniGlobals.h"
#include "LowLevel.h"
#include "OneWireSlave.h"

//----------------------------------------------------------------------------------------------------------------
/*
 Пины, которые использует плата модуля с датчиками:
 
 */

//----------------------------------------------------------------------------------------------------------------
// ПОЛЬЗОВАТЕЛЬСКИЕ НАСТРОЙКИ
//----------------------------------------------------------------------------------------------------------------
#define SOIL_MOISTURE_0_PERCENT    530 // вольтаж для 0% влажности почвы, китайский датчик влажности (0-1023)
#define SOIL_MOISTURE_100_PERCENT  200 // вольтаж для 100% влажности почвы, китайский датчик влажности (0-1023)
//----------------------------------------------------------------------------------------------------------------
#define _DEBUG // раскомментировать для отладочного режима (плюётся в Serial, не использовать с подключённым RS-485 !!!)

//----------------------------------------------------------------------------------------------------------------
// настройки LoRa
//----------------------------------------------------------------------------------------------------------------
#define USE_LORA                        // закомментировать, если не надо работать через LoRa.
/*
 LoRa для своей работы занимает следующие пины: 9,10,11,12,13.
 Следите за тем, чтобы номера пинов не пересекались в слотах, или с RS-485, или ещё где.
 */
#define LORA_SS_PIN             PA4    // пин SS для LoRa
#define LORA_RESET_PIN          PB0    // пин Reset для LoRa
#define LORA_POWER_PIN          PB13    // пин IO0 для LoRa
#define LORA_FREQUENCY        868E6    // частота работы (433E6, 868E6, 915E6)
#define LORA_TX_POWER            17    // мощность передатчика (1 - 17)

 //----------------------------------------------------------------------------------------------------------------
//
//#define PWM_PIN                8;    // номер пина, на котором будем управлять
//unsigned long frequency = 2000000;    // частота  1 - 2000000 (Гц)
//int brightness = 150;    // частота ШИМ (0-255)  


//----------------------------------------------------------------------------------------------------------------
// настройки датчиков для модуля, МЕНЯТЬ ЗДЕСЬ!
//----------------------------------------------------------------------------------------------------------------
const SensorSettings Sensors[3] = {
       
 {mstChinaSoilMoistureMeter,33,0},     // PC14 датчик влажности почвы на пине 
 {mstDS18B20,26,0},                  // PB10
 {mstNone,0, 0}


/* 
 
 поддерживаемые типы датчиков: 
 
 
  {mstDS18B20,A0,0} - датчик DS18B20 на пине A0
  {mstChinaSoilMoistureMeter,A7,0} - китайский датчик влажности почвы на пине A7
 
  // Частотные датчики влажности почвы должны на выходе выдавать ШИМ, по заполнению которого рассчитывается влажность почвы !!! Максимальный коэффициент заполнения - 254, минимальный - 1.
  {mstFrequencySoilMoistureMeter,A5, 0} - частотный датчик влажности почвы на аналоговом пине A5
  {mstFrequencySoilMoistureMeter,A4, 0} - частотный датчик влажности почвы на аналоговом пине A4
  {mstFrequencySoilMoistureMeter,A3, 0} - частотный датчик влажности почвы на аналоговом пине A3
  

  если в слоте записано
    {mstNone,0, 0}
  то это значит, что датчика на этом слоте нет   

 */

};
//----------------------------------------------------------------------------------------------------------------
// Дальше лазить - неосмотрительно :)
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
// ДАЛЕЕ ИДУТ СЛУЖЕБНЫЕ НАСТРОЙКИ И КОД - МЕНЯТЬ С ПОЛНЫМ ПОНИМАНИЕМ ТОГО, ЧТО ХОДИМ СДЕЛАТЬ !!!
//----------------------------------------------------------------------------------------------------------------
// Настройки 1-Wire
Pin oneWireData(PB12); // PB11 на втором пине у нас висит 1-Wire
const byte owROM[7] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 }; // адрес датчика, менять не обязательно, т.к. у нас не честный 1-Wire
// команды 1-Wire
const byte COMMAND_START_CONVERSION = 0x44; // запустить конвертацию
const byte COMMAND_READ_SCRATCHPAD  = 0xBE; // попросили отдать скратчпад мастеру
const byte COMMAND_WRITE_SCRATCHPAD = 0x4E; // попросили записать скратчпад, следом пойдёт скратчпад
const byte COMMAND_SAVE_SCRATCHPAD  = 0x25; // попросили сохранить скратчпад в EEPROM
enum DeviceState {
  DS_WaitingReset,
  DS_WaitingCommand,
  DS_ReadingScratchpad,
  DS_SendingScratchpad
};
volatile DeviceState state                 = DS_WaitingReset;  
volatile byte scratchpadWritePtr           = 0; // указатель на байт в скратчпаде, куда надо записать пришедший от мастера байт
volatile byte scratchpadNumOfBytesReceived = 0; // сколько байт прочитали от мастера

                                                //----------------------------------------------------------------------------------------------------------------
#define ROM_ADDRESS (void*) 123 // по какому адресу у нас настройки?
//----------------------------------------------------------------------------------------------------------------
t_scratchpad scratchpadS, scratchpadToSend;
volatile char* scratchpad = (char *)&scratchpadS;          //что бы обратиться к scratchpad как к линейному массиву

volatile bool scratchpadReceivedFromMaster     = false;    // флаг, что мы получили данные с мастера
volatile bool needToMeasure                    = false;    // флаг, что мы должны запустить конвертацию
volatile unsigned long sensorsUpdateTimer      = 0;        // таймер получения информации с датчиков и обновления данных в скратчпаде
volatile bool measureTimerEnabled              = false;    // флаг, что мы должны прочитать данные с датчиков после старта измерений
volatile unsigned long query_interval          = MEASURE_MIN_TIME; // тут будет интервал опроса
unsigned long last_measure_at                  = 0;        // когда в последний раз запускали конвертацию

volatile bool connectedViaOneWire              = false;    // флаг, что мы присоединены к линии 1-Wire, при этом мы не сорим в эфир по nRF и не обновляем состояние по RS-485
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
void* SensorDefinedData[3] = {NULL}; // данные, определённые датчиками при инициализации
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

    // пишем номер канала по умолчанию
  /*  if(scratchpadS.rf_id == 0xFF || scratchpadS.rf_id == 0)
      scratchpadS.rf_id = DEFAULT_RF_CHANNEL; */

    scratchpadS.packet_type = ptSensorsData; // говорим, что это тип пакета - данные с датчиками
    scratchpadS.packet_subtype = 0;


    // если интервала опроса не сохранено - выставляем по умолчанию
    if(scratchpadS.query_interval_min == 0xFF)
      scratchpadS.query_interval_min = 0;
      
    if(scratchpadS.query_interval_sec == 0xFF)
      scratchpadS.query_interval_sec =  MEASURE_MIN_TIME/1000;

   if(scratchpadS.query_interval_min == 0 && scratchpadS.query_interval_sec < 5) // минимум 5 секунд между обновлениями датчиков
    scratchpadS.query_interval_sec = 5;

    // вычисляем интервал опроса
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

    // смотрим, есть ли у нас калибровка?
    byte calibration_enabled = false;
    for(byte i=0;i<3;i++)
    {
        switch(Sensors[i].Type)
        {
            case mstChinaSoilMoistureMeter:
            {
              calibration_enabled = true;
              // устанавливаем значения по умолчанию
              if(scratchpadS.calibration_factor1 == 0xFF && scratchpadS.calibration_factor2 == 0xFF)
              {
                // в EEPROM ничего нет по этому поводу, оба значения одинаковы
                scratchpadS.calibration_factor1 = map(SOIL_MOISTURE_0_PERCENT,0,1023,0,255);
                scratchpadS.calibration_factor2 = map(SOIL_MOISTURE_100_PERCENT,0,1023,0,255);
              }

              // мы поддерживаем два фактора калибровки
              scratchpadS.config |= (4 | 8);
            }
            break; // mstChinaSoilMoistureMeter

        } // switch

        if(calibration_enabled)
          break;
    
    } // for

    if(calibration_enabled)
    {
      // включён фактор калибровки
      scratchpadS.config |= 2; // устанавливаем второй бит, говоря, что мы поддерживаем калибровку
    } // if
    else
    {
      scratchpadS.config &= ~2; // второй бит убираем по-любому
    }

}
//----------------------------------------------------------------------------------------------------------------
void WakeUpSensor(const SensorSettings& sett, void* sensorDefinedData)
{  
  // просыпаем сенсоры
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
void* InitDS18B20(const SensorSettings& sett) // инициализируем датчик температуры
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

  if(!ow.reset()) // нет датчика
  {
     #ifdef _DEBUG
      Serial.println(F("DS18B20 - not found during init!!!"));
    #endif
    return NULL;  
  }

   ow.write(0xCC); // пофиг на адреса (SKIP ROM)
   ow.write(0x4E); // запускаем запись в scratchpad

   ow.write(0); // верхний температурный порог 
   ow.write(0); // нижний температурный порог
   ow.write(0x7F); // разрешение датчика 12 бит

   ow.reset();
   ow.write(0xCC); // пофиг на адреса (SKIP ROM)
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
  
  // инициализируем датчики
  for(byte i=0;i<3;i++)
    SensorDefinedData[i] = InitSensor(Sensors[i]);
         
}
//----------------------------------------------------------------------------------------------------------------
 void ReadDS18B20(const SensorSettings& sett, struct sensor* s) // читаем данные с датчика температуры
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
    
    if(!ow.reset()) // нет датчика на линии
    {
    #ifdef _DEBUG
      Serial.println(F("DS18B20 - not found!"));
    #endif   
    return; 
    }

  byte data[9] = {0};
  
  ow.write(0xCC); // пофиг на адреса (SKIP ROM)
  ow.write(0xBE); // читаем scratchpad датчика на пине

  for(uint8_t i=0;i<9;i++)
    data[i] = ow.read();


 if (OneWire::crc8(data, 8) != data[8]) // проверяем контрольную сумму
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
  // теперь смотрим отношение highTime к общей длине импульсов - это и будет влажность почвы
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
   
  // теперь, если у нас значение 0% влажности больше, чем значение 100% влажности - надо от 10000 отнять полученное значение
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
  // читаем информацию с датчиков
    
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
    
    if(!ow.reset()) // нет датчика на линии
    {
      #ifdef _DEBUG
        Serial.println(F("DS18B20 - not found!!!"));
      #endif    
      return; 
    }

    ow.write(0xCC);
    ow.write(0x44); // посылаем команду на старт измерений
    
    ow.reset();    

  #ifdef _DEBUG
    Serial.println(F("DS18B20 - converted."));
  #endif    
  
}
//----------------------------------------------------------------------------------------------------------------
bool HasI2CSensors()
{
  // проверяем, есть ли у нас хоть один датчик на I2C
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
void MeasureSensor(const SensorSettings& sett,void* sensorDefinedData) // запускаем конвертацию с датчика, если надо
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
  // обновляем датчики здесь
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
 
  // запускаем конвертацию
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
  
  // инициализируем LoRa
  LoRa.setPins(LORA_SS_PIN,LORA_RESET_PIN,-1);
  loRaInited = LoRa.begin(LORA_FREQUENCY);

  if(loRaInited)
  {
    LoRa.setTxPower(LORA_TX_POWER);
    //LoRa.receive(); // начинаем слушать
    LoRa.sleep(); // засыпаем
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

    for(int i=0;i<5;i++) // пытаемся послать 5 раз
    {
        // подсчитываем контрольную сумму
        scratchpadS.crc8 = OneWireSlave::crc8((const byte*)&scratchpadS,sizeof(scratchpadS)-1);  
        LoRa.beginPacket();
        LoRa.write((byte*)&scratchpadS,sizeof(scratchpadS)); // пишем в эфир
        if(LoRa.endPacket()) // пишем в него
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

  // рандомная задержка
  delay(random(50));

  LoRa.sleep(); // засыпаем

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
      // переназначаем канал радио
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

   InitSensors(); // инициализируем датчики   

    #ifdef USE_LORA
      initLoRa();
    #endif

  oneWireLastCommandTimer = millis();
  
  OWSlave.setReceiveCallback(&owReceive);
  OWSlave.begin(owROM, oneWireData.getPinNumber());


  // InitTimersSafe();                                  //инициализируем все таймеры, кроме 0,
  ////Timer2_Initialize();
  //SetPinFrequencySafe(PWM_PIN, frequency);           //устанавливает частоту для указанного pin
  //SetPinFrequency(PWM_PIN, frequency);               //устанавливает частоту для указанного pin
  //pwmWrite(PWM_PIN, brightness);                     //0-255   используйте эту функцию вместо analogWrite 

  
}
//----------------------------------------------------------------------------------------------------------------
void owSendDone(bool error) {
  UNUSED(error);
 // закончили посылать скратчпад мастеру
 state = DS_WaitingReset;
}
//----------------------------------------------------------------------------------------------------------------
// обработчик прерывания на пине
void owReceive(OneWireSlave::ReceiveEvent evt, byte data)
{
  connectedViaOneWire = true; // говорим, что мы подключены через 1-Wire
  needResetOneWireLastCommandTimer = true; // просим, чтобы сбросили таймер момента получения последней команды
  
  switch (evt)
  {
  case OneWireSlave::RE_Byte:
    switch (state)
    {

     case DS_ReadingScratchpad: // читаем скратчпад от мастера

        // увеличиваем кол-во прочитанных байт
        scratchpadNumOfBytesReceived++;

        // пишем в скратчпад принятый байт
        scratchpad[scratchpadWritePtr] = data;
        // увеличиваем указатель записи
        scratchpadWritePtr++;

        // проверяем, всё ли прочитали
        if(scratchpadNumOfBytesReceived >= sizeof(scratchpadS)) {
          // всё прочитали, сбрасываем состояние на ожидание резета
          state = DS_WaitingReset;
          scratchpadNumOfBytesReceived = 0;
          scratchpadWritePtr = 0;
          scratchpadReceivedFromMaster = true; // говорим, что мы получили скратчпад от мастера
          // вычисляем новый интервал опроса
          //query_interval = (scratchpadS.query_interval_min*60 + scratchpadS.query_interval_sec)*1000;
        }
        
     break; // DS_ReadingScratchpad
      
    case DS_WaitingCommand:
      switch (data)
      {
      case COMMAND_START_CONVERSION: // запустить конвертацию
        state = DS_WaitingReset;
        if(!measureTimerEnabled && !needToMeasure) // только если она уже не запущена
          needToMeasure = true;
        break;

      case COMMAND_READ_SCRATCHPAD: // попросили отдать скратчпад мастеру
        state = DS_SendingScratchpad;
        OWSlave.beginWrite((const byte*)&scratchpadToSend, sizeof(scratchpadToSend), owSendDone);
        break;

      case COMMAND_WRITE_SCRATCHPAD:  // попросили записать скратчпад, следом пойдёт скратчпад
          state = DS_ReadingScratchpad; // ждём скратчпада
          scratchpadWritePtr = 0;
          scratchpadNumOfBytesReceived = 0;
        break;

        case COMMAND_SAVE_SCRATCHPAD: // сохраняем скратчпад в память
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

    
    // скратч был получен от мастера, тут можно что-то делать
    memcpy(&scratchpadToSend,&scratchpadS,sizeof(scratchpadS));
    scratchpadToSend.crc8 = OneWireSlave::crc8((const byte*) &scratchpadToSend,sizeof(scratchpadS)-1);

    // вычисляем новый интервал опроса
    query_interval = (scratchpadToSend.query_interval_min*60 + scratchpadToSend.query_interval_sec)*1000;
          
     #ifdef _DEBUG
        Serial.println(F("Scratch received from master!"));
     #endif
      
  } // scratchpadReceivedFromMaster

  
  unsigned long curMillis = millis();

  // если попросили сбросить таймер получения последней команды по линии 1-Wire - делаем это
  if(needResetOneWireLastCommandTimer) 
  {
    oneWireLastCommandTimer = curMillis;
    needResetOneWireLastCommandTimer = false;
  }

  // проверяем - когда приходила последняя команда по 1-Wire: если её не было больше 15 секунд - активируем nRF и RS-485
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
        
          connectedViaOneWire = false; // соединение через 1-Wire разорвано
      }
  }
  


  if(!connectedViaOneWire && ((curMillis - last_measure_at) > query_interval) && !measureTimerEnabled && !needToMeasure) 
  {
    // чего-то долго не запускали конвертацию, запустим, пожалуй
      // и запустим только тогда, когда мы не подключены к 1-Wire, иначе - мастер сам запросит конвертацию.
        needToMeasure = true;
        #ifdef _DEBUG
          Serial.println(F("Want measure by timeout..."));
        #endif        
  }

  // только если ничего не делаем на линии 1-Wire и запросили конвертацию
  
  if(needToMeasure && !measureTimerEnabled) 
  {
    #ifdef _DEBUG
      Serial.println(F("Want measure..."));
    #endif    

    measureTimerEnabled = true; // включаем флаг, что мы должны прочитать данные с датчиков
    sensorsUpdateTimer = curMillis; // сбрасываем таймер обновления
    StartMeasure();

    needToMeasure = false;
    
    #ifdef _DEBUG
      Serial.println(F("Wait for measure complete..."));
    #endif    
  }
 

  if(measureTimerEnabled) 
  {
    UpdateSensors(); // обновляем датчики, если кому-то из них нужно периодическое обновление
  }
  
  if(measureTimerEnabled && ((curMillis - sensorsUpdateTimer) > MEASURE_MIN_TIME)) 
  {
    
    if(state != DS_SendingScratchpad)
    {

          #ifdef _DEBUG
            Serial.println(F("Measure completed, start read..."));
          #endif
        
             // можно читать информацию с датчиков
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
