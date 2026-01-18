/*
 !!Вариант адаптации STM32 IDE Arduino STM32GENERIC  https://github.com/danieleff/STM32GENERIC


Библиотека дисплея https://github.com/ZinggJM/GxTFT
Подключение дисплея по протоколу FSMC 16 бит
Подключение тачсрина по SPI2

Загрузка в режиме BLACK F407VG/VE/ZG boards, опция Specific board: 
BLACK F407ZG (M4 DEMO)
BLACK F407VG


Вывод Serial в USB порт при настройках при прошивке микроконтроллера
Serial communication: SerialUART1
Upload method: STLink[Automatic Serial= Serial]
USB: Serial[Virtual COM port, PA11/PA12 pins] 
 */

 
#include "Globals.h"

#include "CommandBuffer.h" 
#include "CommandParser.h"
#include "ModuleController.h"
#include "AlertModule.h" 
#include "ZeroStreamListener.h"
#include "Memory.h"
#include "InteropStream.h"
#include "PowerManager.h"
#include "itoa.h"

#ifdef USE_SCENE_MODULE
#include "SceneModule.h"
#endif

#ifdef USE_HTTP_MODULE
#include "HttpModule.h"
#endif

#ifdef USE_PIN_MODULE
#include "PinModule.h"
#endif

#ifdef USE_STAT_MODULE
#include "StatModule.h"
#endif

#ifdef USE_TEMP_SENSORS
#include "TempSensors.h"
#endif

#ifdef USE_SMS_MODULE
#include "SMSModule.h"
#endif

#ifdef USE_WATERING_MODULE
#include "WateringModule.h"
#endif

#ifdef USE_LUMINOSITY_MODULE
#include "LuminosityModule.h"
#endif

#ifdef USE_HUMIDITY_MODULE
#include "HumidityModule.h"
#endif

#ifdef USE_WIFI_MODULE
#include "WiFiModule.h"
#endif

#ifdef USE_LOG_MODULE
#include "LogModule.h"
#endif

#ifdef USE_DELTA_MODULE
#include "DeltaModule.h"
#endif


#ifdef USE_WATERFLOW_MODULE
#include "WaterflowModule.h"
#endif

#ifdef USE_COMPOSITE_COMMANDS_MODULE
#include "CompositeCommandsModule.h"
#endif

#ifdef USE_SOIL_MOISTURE_MODULE
#include "SoilMoistureModule.h"
#endif

#ifdef USE_RESERVATION_MODULE
#include "ReservationModule.h"
#endif

#ifdef USE_TIMER_MODULE
#include "TimerModule.h"
#endif

#ifdef USE_PH_MODULE
#include "PHModule.h"
#endif

#ifdef USE_CO2_MODULE
#include "CO2Module.h"
#endif

#ifdef USE_EC_MODULE
#include "ECModule.h"
#endif

#ifdef USE_IOT_MODULE
#include "IoTModule.h"
#endif

#ifdef USE_TFT_MODULE
#include "TFTModule.h"
#endif

#ifdef USE_BUZZER
#include "Buzzer.h"
#endif

#ifdef USE_MCP_MODULE
#include "MCPModule.h"
#endif


///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////
#ifdef USE_DYNAMIC_SENSORS_RESET_MODULE
#include "DynamicSensorsResetModule.h"
#endif
///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////

#ifdef USE_DOOR_MODULE
#include "DoorModule.h"
#endif

#ifdef USE_SCHEDULE_MODULE
#include "ScheduleModule.h"
#endif

#ifdef USE_WATER_TANK_MODULE
#include "WaterTankModule.h"
#endif

#if defined(USE_WIFI_MODULE) || defined(USE_SMS_MODULE)
#include "CoreTransport.h"
#endif


unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout)
{
  // cache the port and bit of the pin in order to speed up the
  // pulse width measuring loop and achieve finer resolution.  calling
  // digitalRead() instead yields much coarser resolution.
  uint8_t bit = digitalPinToBitMask(pin);
  GPIO_TypeDef* port = digitalPinToPort(pin);
  uint8_t stateMask = (state ? bit : 0);

  unsigned long startMicros = micros();

  // wait for any previous pulse to end
  while ((*portInputRegister(port) & bit) == stateMask) {
    if (micros() - startMicros > timeout)
      return 0;
  }

  // wait for the pulse to start
  while ((*portInputRegister(port) & bit) != stateMask) {
    if (micros() - startMicros > timeout)
      return 0;
  }

  unsigned long start = micros();
  // wait for the pulse to stop
  while ((*portInputRegister(port) & bit) == stateMask) {
    if (micros() - startMicros > timeout)
      return 0;
  }
  return micros() - start;
}

void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
  uint8_t i;

  for (i = 0; i < 8; i++)  {
    if (bitOrder == LSBFIRST)
      digitalWrite(dataPin, !!(val & (1 << i)));
    else  
      digitalWrite(dataPin, !!(val & (1 << (7 - i))));
      
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);    
  }
}

#include "LogicManageModule.h"
#include "EEPROMSettingsModule.h"

#include "DelayedEvents.h"

#include<Wire.h>
#include <SPI.h>

#ifdef STM32_START_DEBUG
#define START_LOG(x) {Serial4.println((x)); Serial4.flush(); }
#else
#define START_LOG(x) (void) 0
#endif


// Ждем команды из сериала
CommandBuffer commandsFromSerial(&Serial);

// Парсер команд
CommandParser commandParser;

// Контроллер модулей
ModuleController controller;

#ifdef USE_PIN_MODULE
//  Модуль управления цифровыми пинами
PinModule pinModule;
#endif

#ifdef USE_STAT_MODULE
// Модуль вывода статистики
StatModule statModule;
#endif

#ifdef USE_TEMP_SENSORS
// модуль опроса температурных датчиков и управления фрамугами
TempSensors tempSensors;
#endif

#ifdef USE_IOT_MODULE
// модуль отсыла данных на внешние IoT-хранилища
IoTModule iotModule; 
#endif

#ifdef USE_HTTP_MODULE
// модуль проверки на наличие входящих команд
HttpModule httpModule;
#endif

#ifdef USE_SMS_MODULE
// модуль управления по SMS
 SMSModule smsModule;
#endif

#ifdef USE_WATERING_MODULE
// модуль управления поливом
WateringModule wateringModule;
#endif

#ifdef USE_LUMINOSITY_MODULE
// модуль управления досветкой и получения значений освещённости
LuminosityModule luminosityModule;
#endif

#ifdef USE_HUMIDITY_MODULE
// модуль работы с датчиками влажности DHT
HumidityModule humidityModule;
#endif

#ifdef USE_LOG_MODULE
// модуль логгирования информации
LogModule logModule;
#endif

#ifdef USE_DELTA_MODULE
// модуль сбора дельт с датчиков
DeltaModule deltaModule;
#endif

#ifdef USE_WATERFLOW_MODULE
// модуль учёта расхода воды
WaterflowModule waterflowModule;
#endif

#ifdef USE_COMPOSITE_COMMANDS_MODULE
// модуль составных команд
CompositeCommandsModule compositeCommands;
#endif

#ifdef USE_SOIL_MOISTURE_MODULE
// модуль датчиков влажности почвы
SoilMoistureModule soilMoistureModule;
#endif

#ifdef USE_PH_MODULE
// модуль контроля pH
PhModule phModule;
#endif

#ifdef USE_CO2_MODULE
// модуль контроля CO2
CO2Module co2Module;
#endif

#ifdef USE_EC_MODULE
// модуль контроля EC
ECModule ecModule;
#endif

#ifdef USE_RESERVATION_MODULE
ReservationModule reservationModule;
#endif

#ifdef USE_MCP_MODULE
MCPModule mcpModule;
#endif


#ifdef USE_TIMER_MODULE
TimerModule timerModule;
#endif

#ifdef USE_TFT_MODULE
TFTModule tftModule;
#endif

#ifdef USE_DOOR_MODULE
DoorModule doorModule;
#endif


#ifdef USE_SCHEDULE_MODULE
ScheduleModule scheduleModule;
#endif

#ifdef USE_WATER_TANK_MODULE
WaterTankModule waterTankModule;
#endif



BlinkModeInterop readyDiodeBlinker;
bool canWorkWithReadyDiode = false;

#ifdef USE_WIFI_MODULE
// модуль работы по Wi-Fi
WiFiModule wifiModule;
#endif

#ifdef USE_SCENE_MODULE
SceneModule sceneModule;
#endif

///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////
#ifdef USE_DYNAMIC_SENSORS_RESET_MODULE
DynamicSensorsResetModule dynamicResetSensors;
#endif
///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////

LogicManageModuleClass logicManageModule;
EEPROMSettingsModule eepromSettingsModule;
ZeroStreamListener zeroStreamModule;
AlertModule alertsModule;

#ifdef USE_EXTERNAL_WATCHDOG
  typedef enum
  {
    WAIT_FOR_TRIGGERED,
    WAIT_FOR_NORMAL 
  } ExternalWatchdogState;
  
  typedef struct
  {
    uint16_t timer;
    ExternalWatchdogState state;
  } ExternalWatchdogSettings;

  ExternalWatchdogSettings watchdogSettings;
#endif
//--------------------------------------------------------------------------------------------------------------------------------
bool canCallYield = false;
//--------------------------------------------------------------------------------------------------------------------------------
void initI2C()
{

    // инициализация для STM32
      Wire.begin();
  //    Wire.setClock(MY_I2C_SPEED);

      #if(DS3231_WIRE_NUMBER == 1)
      {
        Wire1.begin();
//        Wire1.setClock(MY_I2C_SPEED);
      }
      #endif
     
}
//--------------------------------------------------------------------------------------------------------------------------------
void doResetI2C(uint8_t sclPin, uint8_t sdaPin)
{
  pinMode(sdaPin, OUTPUT);
  digitalWrite(sdaPin,HIGH);
  pinMode(sclPin,OUTPUT);
  
  for(uint8_t i=0;i<10;i++) // Send NACK signal
  {
    digitalWrite(sclPin,HIGH);
    delayMicroseconds(5);
    digitalWrite(sclPin,LOW);
    delayMicroseconds(5);   
  }

  // Send STOP signal
  digitalWrite(sdaPin,LOW);
  delayMicroseconds(5);
  digitalWrite(sclPin,HIGH);
  delayMicroseconds(2);
  digitalWrite(sdaPin,HIGH);
  delayMicroseconds(2);
  
  
  pinMode(sclPin,INPUT);   
  pinMode(sdaPin,INPUT);   
}
//--------------------------------------------------------------------------------------------------------------------------------
void resetI2C()
{
  doResetI2C(SCL, SDA);
  
 #if TARGET_BOARD == STM32_BOARD
    #if(DS3231_WIRE_NUMBER == 1)
      doResetI2C(SCL1, SDA1);     
    #endif
 #endif 
}
//--------------------------------------------------------------------------------------------------------------------------------
void initSPI()
{
  #if defined(USE_NRF_GATE) || defined(USE_LORA_GATE) || defined(USE_MCP23S17_EXTENDER)
    SPI.begin();    
  #endif
}
//--------------------------------------------------------------------------------------------------------------------------------
// Принудительная очистка внешней памяти настроек в EEPROM

bool extended_EEPROM_reset = false;

void clearExtenderEEPRON()
{
    pinMode(PIN_EXTENDED_EEPROM_RESET, INPUT);
    int var = 0;

    if (digitalRead(PIN_EXTENDED_EEPROM_RESET) == LOW)
    {
        Serial.println(F("Please wait for number 10"));
    }

    while (digitalRead(PIN_EXTENDED_EEPROM_RESET) == LOW)
    {
        // выполнить что-то, повторив N раз
        var++;
        delay(1000);

        if (var > 10)
        {
            extended_EEPROM_reset = true;
            //break;
        }
        if (extended_EEPROM_reset == true)
        {
            Serial.println(F("Start EEPROM clearance..."));

            TFTScreen->switchToScreen("MEMINIT");
            tftModule.Update();

            //MemClear();
            //Serial.println(F("Please restart controller."));
            extended_EEPROM_reset = false;
            break;//while (1);
        }

        char cstr[5];
        itoa(var, cstr, 10);
        Vector<const char*> lines;
        lines.push_back("ОЖИДАНИЕ ВЫЗОВА ПРОГРАММЫ");
        lines.push_back("ОЧИСТКИ НАСТРОЕК");
        lines.push_back("Удерживать кнопку 'СТЕРЕТЬ'");
        lines.push_back("до значения больше 10");
        lines.push_back("");
        lines.push_back(cstr);


        MessageBox->show_noButton("ОЧИСТКА НАСТРОЕК", lines);
        tftModule.Update();

        Serial.print(F("Look for..."));
        Serial.println(var);
    }
}


//--------------------------------------------------------------------------------------------------------------------------------
// эта функция вызывается после обновления состояния каждого модуля.
// передаваемый параметр - указатель на обновлённый модуль.
// если модулю предстоит долгая работа - помимо этого инструмента
// модуль должен дёргать функцию yield, если чем-то долго занят!
//--------------------------------------------------------------------------------------------------------------------------------
void ModuleUpdateProcessed(AbstractModule* module)
{
    UNUSED(module);

    CoreDelayedEvent.update();

    // используем её, чтобы проверить состояние порта UART для WI-FI-модуля - вдруг надо внеочередно обновить
#ifdef USE_WIFI_MODULE
// модуль Wi-Fi обновляем каждый раз после обновления очередного модуля
    ESP.update();
#endif

#ifdef USE_SMS_MODULE
    // и модуль GSM тоже тут обновим
    SIM800.update();
#endif     
}
//--------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_EXTERNAL_WATCHDOG
void updateExternalWatchdog()
{
    static unsigned long watchdogLastMillis = millis();
    unsigned long watchdogCurMillis = millis();

    uint16_t dt = watchdogCurMillis - watchdogLastMillis;
    watchdogLastMillis = watchdogCurMillis;

    watchdogSettings.timer += dt;
    switch (watchdogSettings.state)
    {
    case WAIT_FOR_TRIGGERED:
    {
        if (watchdogSettings.timer >= WATCHDOG_WORK_INTERVAL)
        {
            watchdogSettings.timer = 0;
            watchdogSettings.state = WAIT_FOR_NORMAL;
            digitalWrite(WATCHDOG_REBOOT_PIN, WATCHDOG_TRIGGERED_LEVEL);
        }
    }
    break;

    case WAIT_FOR_NORMAL:
    {
        if (watchdogSettings.timer >= WATCHDOG_PULSE_DURATION)
        {
            watchdogSettings.timer = 0;
            watchdogSettings.state = WAIT_FOR_TRIGGERED;
            digitalWrite(WATCHDOG_REBOOT_PIN, WATCHDOG_NORMAL_LEVEL);
        }
    }
    break;
    }

}
#endif
//--------------------------------------------------------------------------------------------------------------------------------
void processCommandsFromSerial()
{
    // смотрим, есть ли входящие команды
    if (commandsFromSerial.HasCommand())
    {
        // есть новая команда
        Command cmd;
        if (commandParser.ParseCommand(commandsFromSerial.GetCommand(), cmd))
        {
            Stream* answerStream = commandsFromSerial.GetStream();
            // разобрали, назначили поток, с которого пришла команда
            cmd.SetIncomingStream(answerStream);

            // запустили команду в обработку
            controller.ProcessModuleCommand(cmd);

        } // if
        else
        {
            Serial.println("ProcessModuleCommand failure!!!");
            // что-то пошло не так, игнорируем команду
        } // else

        commandsFromSerial.ClearCommand(); // очищаем полученную команду
    } // if  
}

//SD_HandleTypeDef hsd;

//--------------------------------------------------------------------------------------------------------------------------------
void setup() 
{
  canCallYield = false;

  //HAL_SD_DeInit(&hsd);

  //// Если использовался DMA - вручную отключить
  ////__HAL_RCC_DMA2_FORCE_RESET();
  ////__HAL_RCC_DMA2_RELEASE_RESET();

  ////// Проинициализировать SD-пины вручную в "Analog" (если CubeMX позволяет)
  ////GPIO_InitTypeDef GPIO_InitStruct = { 0 };
  ////GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  ////GPIO_InitStruct.Pull = GPIO_NOPULL;

  ////// SDio D0-D3, CMD, CLK --> свой порт/пины исправьте!
  ////GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
  ////HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  ////GPIO_InitStruct.Pin = GPIO_PIN_2; HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  ////// SD_CK скорее всего на PC12, SD_CMD на PD2 и так далее, подберите ваши!

  ////// После этого сделать остальной сброс:
  ////HAL_RCC_DeInit();
  ////HAL_DeInit();

  Serial.begin(SERIAL_BAUD_RATE);        // запускаем Serial на нужной скорости
  Serial4.begin(SERIAL_BAUD_RATE);       // запускаем Serial4 на нужной скорости
  delay(500);
  while (!Serial && millis() < 1000);    // ждём инициализации Serial
  while (!Serial4 && millis() < 1000);   // ждём инициализации Serial4

  Serial.println(F("Start controller."));
  Serial4.println(F("Start controller."));
 
  String ver_soft = __FILE__;
  int val_srt = ver_soft.lastIndexOf('\\');
  ver_soft.remove(0, val_srt + 1);
  val_srt = ver_soft.lastIndexOf('.');
  ver_soft.remove(val_srt);
  Serial.println(ver_soft);
  Serial4.println(ver_soft);

  START_LOG(1);
   
  #if TARGET_BOARD == STM32_BOARD
    WORK_STATUS.PinMode(20,INPUT,false);
    WORK_STATUS.PinMode(21,OUTPUT,false);
  #else
    WORK_STATUS.PinMode(SDA,INPUT,false);
    WORK_STATUS.PinMode(SCL,OUTPUT,false);
  #endif

  START_LOG(2);

  resetI2C();

  START_LOG(3);

  initI2C();  

   START_LOG(4);

   initSPI();

  // инициализируем память (EEPROM не надо, а вот I2C - надо)
  MemInit(); 

  START_LOG(5);

  WORK_STATUS.PinMode(0,INPUT,false);
  WORK_STATUS.PinMode(1,OUTPUT,false);

  START_LOG(6);

  #ifdef USE_EXTERNAL_WATCHDOG
    WORK_STATUS.PinMode(WATCHDOG_REBOOT_PIN,OUTPUT,true);
    digitalWrite(WATCHDOG_REBOOT_PIN,WATCHDOG_NORMAL_LEVEL);

    watchdogSettings.timer = 0;
    watchdogSettings.state = WAIT_FOR_TRIGGERED;
  #endif
 
  
  // настраиваем все железки
  controller.Setup();

 
  START_LOG(8);
   
  // устанавливаем провайдера команд для контроллера
  controller.SetCommandParser(&commandParser);

  START_LOG(9);

  controller.RegisterModule(&eepromSettingsModule);  
  // настраиваем менеджер питания
  PowerManager.begin();
  
  #ifdef USE_BUZZER
  Buzzer.begin();
  #endif
  
  // регистрируем модули  
  #ifdef USE_PIN_MODULE  
  controller.RegisterModule(&pinModule);
  #endif

  START_LOG(10);
  
  #ifdef USE_STAT_MODULE
  controller.RegisterModule(&statModule);
  #endif

  START_LOG(11);

  #ifdef USE_TEMP_SENSORS
  controller.RegisterModule(&tempSensors);
  #endif

  START_LOG(12);

  #ifdef USE_WATERING_MODULE
  controller.RegisterModule(&wateringModule);
  #endif

  START_LOG(13);

  #ifdef USE_LUMINOSITY_MODULE
  controller.RegisterModule(&luminosityModule);
  #endif

  START_LOG(14);

  #ifdef USE_HUMIDITY_MODULE
  controller.RegisterModule(&humidityModule);
  #endif

  START_LOG(15);

  #ifdef USE_DELTA_MODULE
  controller.RegisterModule(&deltaModule);
  #endif

  START_LOG(16);

  #ifdef USE_MCP_MODULE
  controller.RegisterModule(&mcpModule);
  #endif
  
 
  START_LOG(18);

  #ifdef USE_WATERFLOW_MODULE
  controller.RegisterModule(&waterflowModule);
  #endif

  START_LOG(19);

  #ifdef USE_COMPOSITE_COMMANDS_MODULE
  controller.RegisterModule(&compositeCommands);
  #endif

  START_LOG(20);
 
  #ifdef USE_SOIL_MOISTURE_MODULE
  controller.RegisterModule(&soilMoistureModule);
  #endif

  START_LOG(21);

  #ifdef USE_PH_MODULE
  controller.RegisterModule(&phModule);
  #endif

  START_LOG(22);

  #ifdef USE_CO2_MODULE
  controller.RegisterModule(&co2Module);
  #endif  
  
#ifdef USE_EC_MODULE
  controller.RegisterModule(&ecModule);
#endif  


  START_LOG(23);

  #ifdef USE_RESERVATION_MODULE
  controller.RegisterModule(&reservationModule);
  #endif

  START_LOG(24);

  #ifdef USE_TIMER_MODULE
  controller.RegisterModule(&timerModule);
  #endif

  #ifdef USE_SCENE_MODULE
  controller.RegisterModule(&sceneModule);
  #endif

///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////
  #ifdef USE_DYNAMIC_SENSORS_RESET_MODULE
  controller.RegisterModule(&dynamicResetSensors);
  #endif
///////////////////////////// КОД СБРОСА ДИНАМИЧЕСКИХ ДАТЧИКОВ ////////////////////////////////////////    

  #ifdef USE_DOOR_MODULE
  controller.RegisterModule(&doorModule);
  #endif

   START_LOG(29);

#ifdef USE_WATER_TANK_MODULE

  controller.RegisterModule(&waterTankModule);
#endif

  #ifdef USE_LOG_MODULE
  controller.RegisterModule(&logModule);
  controller.SetLogWriter(&logModule); // задаём этот модуль как модуль, который записывает события в лог
  #endif

  START_LOG(26);

  // модуль Wi-Fi регистрируем до модуля SMS, поскольку Wi-Fi дешевле, чем GPRS, для отсыла данных в IoT-хранилища
  #ifdef USE_WIFI_MODULE
  controller.RegisterModule(&wifiModule);
  #endif 

  START_LOG(27);

  #ifdef USE_SMS_MODULE
  controller.RegisterModule(&smsModule);
  #endif

  START_LOG(28);

  #ifdef USE_IOT_MODULE
    controller.RegisterModule(&iotModule);
  #endif

  START_LOG(29);

  #ifdef USE_HTTP_MODULE
    controller.RegisterModule(&httpModule);
  #endif

  START_LOG(30);

  #ifdef USE_TFT_MODULE
    controller.RegisterModule(&tftModule);
  #endif

 START_LOG(31);

  controller.RegisterModule(&zeroStreamModule);
  controller.RegisterModule(&logicManageModule);

 START_LOG(32);

 // модуль алертов регистрируем последним, т.к. он должен вычитать зависимости с уже зарегистрированными модулями
  controller.RegisterModule(&alertsModule);

 START_LOG(33);

#ifdef USE_SCHEDULE_MODULE // регистрируем модуль расписаний последним, чтобы он обновлялся последним, и сообщал уже обновившим своё состояние модулям о возможных сменах настроек
 controller.RegisterModule(&scheduleModule);
#endif


 TFTScreen->setVer(ver_soft);  // Сохранить строку с текущей версией.

 controller.begin(); // начинаем работу

 START_LOG(34);

     #ifdef SD_USED
    if(controller.SDSupported() && !controller.HasSDCard())
    {
      Serial.println(F("NO SD FOUND!"));
       
      #ifdef USE_TFT_MODULE
       Vector<const char*> lines;
       lines.push_back("НЕ НАЙДЕНА SD-КАРТОЧКА!");
       lines.push_back("");
       lines.push_back("ВСТАВЬТЕ КАРТОЧКУ И ПЕРЕЗАГРУЗИТЕ КОНТРОЛЛЕР.");
       MessageBox->halt("СООБЩЕНИЕ",lines,true,true);
       tftModule.Update();
      #else
        while(1);
      #endif
    }
  #endif // SD_USED
    START_LOG(35);
    clearExtenderEEPRON();                  // Проверить нажатие принудительной очистки внешней EEPROM
    
    START_LOG(36);
        // тут проверяем на инициализацию EEPROM
    uint8_t controlByte = MemRead(0);

    if (controlByte != MEM_CONTROL_BYTE || extended_EEPROM_reset == true)
    {
#ifdef USE_TFT_MODULE
        TFTScreen->switchToScreen("MEMINIT");
        tftModule.Update();
#else
        Serial.println(F("Start EEPROM clearance..."));
        MemClear();
        Serial.println(F("Please restart controller."));
        while (1);
#endif
        extended_EEPROM_reset = false;
    }

  // пискнем при старте, если есть баззер
  #ifdef USE_BUZZER
  Buzzer.buzz();
  #endif

  
  // настраиваем информационные диоды
  DiodesBinding bnd = HardwareBinding->GetDiodesBinding();
  if (bnd.LinkType != linkUnbinded)
  {
      if (bnd.LinkType == linkDirect)
      {
          if (bnd.ReadyDiodePin != UNBINDED_PIN && bnd.ReadyDiodePin > 1)
          {
#ifndef DISABLE_DIODES_CONFIGURE
              if (EEPROMSettingsModule::SafePin(bnd.ReadyDiodePin))
#endif
              {
                  canWorkWithReadyDiode = true;
              }
          }
      }
 
      START_LOG(40);

      if (canWorkWithReadyDiode)
      {
          readyDiodeBlinker.begin(bnd.ReadyDiodePin);

          if (bnd.BlinkReadyDiode == 1)
          {
              readyDiodeBlinker.blink(READY_DIODE_BLINK_INTERVAL);
          }
          else
          {
              canWorkWithReadyDiode = false;
              readyDiodeBlinker.out(bnd.Level); // включаем светодиод при старте
          }
      }

  } // if(bnd.LinkType != linkUnbinded)
  
#ifdef USE_LOG_MODULE
  controller.Log(&logModule, READY); // печатаем в файл действий строчку Ready, которая скажет нам, что контроллер начал работу
#endif
    // Печатаем в Serial готовность
  Serial.print(READY);

#ifdef USE_DS3231_REALTIME_CLOCK

  RealtimeClock rtc = controller.GetClock();
  RTCTime tm = rtc.getTime();

  Serial.print(F(", "));
  Serial.print(rtc.getDayOfWeekStr(tm));
  Serial.print(F(" "));
  Serial.print(rtc.getDateStr(tm));
  Serial.print(F(" - "));
  Serial.print(rtc.getTimeStr(tm));
  Serial.println();
#endif 
  Serial4.print("READY");

 // digitalWrite(44, HIGH);
 canCallYield = true;
  
}

//--------------------------------------------------------------------------------------------------------------------------------
void loop() 
{
// отсюда можно добавлять любой сторонний код
//uint32_t start = millis();
// до сюда можно добавлять любой сторонний код

   #ifdef USE_EXTERNAL_WATCHDOG
     updateExternalWatchdog();
   #endif // USE_EXTERNAL_WATCHDOG
    
	
    if(canWorkWithReadyDiode)
    {
      readyDiodeBlinker.update();   
    }

  // смотрим, есть ли входящие команды
   processCommandsFromSerial();
    
   // обновляем состояние всех зарегистрированных модулей
   controller.UpdateModules(ModuleUpdateProcessed);

   CoreDelayedEvent.update();

   
// отсюда можно добавлять любой сторонний код
//uint32_t end = millis() - start;
//Serial4.print("One loop() pass time, ms: ");
//Serial4.println(end);
// до сюда можно добавлять любой сторонний код

}
//--------------------------------------------------------------------------------------------------------------------------------
void esp_sim800_call()
{
   #ifdef USE_WIFI_MODULE
    ESP.readFromStream();
    #endif

   #ifdef USE_SMS_MODULE
   SIM800.readFromStream();
   #endif   
}
//--------------------------------------------------------------------------------------------------------------------------------
// обработчик простоя, используем и его. Если сторонняя библиотека устроена правильно - она будет
// вызывать yield периодически - этим грех не воспользоваться, чтобы избежать потери данных
// в портах UART. 
//--------------------------------------------------------------------------------------------------------------------------------
void yield()
{
 
  if(!canCallYield)
    return;
    
// отсюда можно добавлять любой сторонний код, который надо вызывать, когда МК чем-то долго занят (например, чтобы успокоить watchdog)


// до сюда можно добавлять любой сторонний код

   #ifdef USE_EXTERNAL_WATCHDOG
     updateExternalWatchdog();
   #endif // USE_EXTERNAL_WATCHDOG

   esp_sim800_call();

   CoreDelayedEvent.update();


// отсюда можно добавлять любой сторонний код, который надо вызывать, когда МК чем-то долго занят (например, чтобы успокоить watchdog)

// до сюда можно добавлять любой сторонний код

}
//--------------------------------------------------------------------------------------------------------------------------------
//void SerialEvent1()
//{
//  esp_sim800_call();
//}
////--------------------------------------------------------------------------------------------------------------------------------
//void SerialEvent2()
//{
//  esp_sim800_call();
//}
////--------------------------------------------------------------------------------------------------------------------------------
//void SerialEvent3()
//{
//   esp_sim800_call();
//}
////--------------------------------------------------------------------------------------------------------------------------------
//
