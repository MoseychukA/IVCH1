//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/*
ООО "Децима"
 Программа управления индикатором LCD2004 (20x4)
 Драйвер индикатора SSD1311  I2C
 Печатная плата LCD2004_PCBmodule6_1 (Proteus)
 
 Среда VisualStudio2019 с расширением VisualMicro
 Поддержка Arduino IDE с опцией STM32GENERIC

 Настройки компиляции в среде  vMicro
 Плата:                 BluePill103C
 Serial commanication:  SerialUSB
 Upload method:         STLink
 Начало программирования проекта 01.04.2022г.
 Начало программирования текущей версии 03.03.2023г.
 */


 // Visual Micro is in vMicro>General>Tutorial Mode
 /*
     Name:      LCD2004_STM32_SSD1311_INA2023_03_03_01.ino
     Created:	03.03.2023 09:10:12
     Author:     DECIMA\moseichuk
 */

 //          WINSTAR Display Co.,Ltd 
 //=============================================================
 //          WINSTAR Display Co.,Ltd
 //    OLED     	  :WEO2002A
 //    Contraller   : SSD1311
 //    history      : version 1.0
 //==============================================================



#include <stdio.h>                // define I/O functions
#include <Arduino.h>              // define I/O functions
#include "Configuration_STM32.h"  // Основные настройки программы
#include "Settings.h"             // дополнительные настройки программы
#include "CoreButton.h"           // обработчик кнопок
#include "CoreCommandBuffer.h"    // обработчик входящих по UART команд
#include "Memory.h"               // Работа с энергонезависимой памятью
#include <Wire.h>                 //
#include "LCDModule.h"            // Работа с дисплеем

LCDModule tftModule;

//--------------------------------------------------------------------------------------------------------------------------------
bool canCallYield = false;
//--------------------------------------------------------------------------------------------------------------------------------
bool lcd_ON              = false;
uint32_t screenIdleTimer = 0;
uint32_t backlightTimer  = 0;
uint32_t powerOffTimer   = 0;
bool power_ON            = false;

//--------------------------------------------------------------------------------------------------------------------------------
void screenAction(AbstractLCDScreen* screen)
{
    // какое-то действие на экране произошло.
    // тут просто сбрасываем таймер ничегонеделанья.
    screenIdleTimer = millis();            // Таймер переключения на главный экран
    backlightTimer  = millis();            // Таймер отключения подсветки дисплея
  //  DBGLN("screenAction");               // Контроль действия подпрограммы
}
//--------------------------------------------------------------------------------------------------------------------------------

void resetI2C()                         // Выполнить сброс I2C
{

    pinMode(SDA, OUTPUT);
    digitalWrite(SDA, HIGH);
    pinMode(SCL, OUTPUT);

    for (uint8_t i = 0; i < 10; i++)    // Send NACK signal
    {
        digitalWrite(SCL, HIGH);
        delayMicroseconds(5);
        digitalWrite(SCL, LOW);
        delayMicroseconds(5);
    }

    // Send STOP signal
    digitalWrite(SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL, HIGH);
    delayMicroseconds(2);
    digitalWrite(SDA, HIGH);
    delayMicroseconds(2);

    pinMode(SCL, INPUT);
    pinMode(SDA, INPUT);
}


byte Opt_Devices = 0;                      // программа автоматического определения типа оптического датчика
byte error, address;
int reset_lcd = 2;


byte Opt_address(byte address)            // программа автоматического определения типа оптического датчика (определение адреса оптического датчика)
{
    digitalWrite(reset_lcd, HIGH);
    delay(20);
    digitalWrite(reset_lcd, HIGH);
    delay(20);
    digitalWrite(reset_lcd, HIGH);
    delay(100);

    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) 
    {
        return address;
    }
    else if (error == 4) 
    {
        return 0;
    }
}


void set_flag_POINT()
{
	Settings.set_checkDeviceOff(2);                           // Установить флаг режима отключения устройства
}

void set_flag_LED_GREEN_HL1()
{
    Settings.set_checkDeviceOff(3);                           // Установить флаг режима отключения устройства
}
void set_flag_GSM_GREEN_LED()
{
    Settings.set_checkDeviceOff(4);                           // Установить флаг режима отключения устройства
   // Settings.set_LED_GPS(true);                             // Зеленый светодиод GPS  включен
}

/*  Переменные для обнаружения сигнала SOS   */

#define PREAMBLE_HIGH          100000        //  Среднее HIGH в преамбуле, 100 мс
#define PRECISION               10000        //  Точность при отклонении от длины сигнала в мкс., чем меньше число, тем строже фильтр, не реккомендуется выставлять менее 10 мкс.

// через volatile, так как изменяется через обработчик прерываний и должна быть в ОЗУ
volatile boolean  SOS_Listening  = false;    //  Переменная - флаг получения пакета SOS
volatile uint32_t SOS_StartPulse = 0;        //  Метка времени, на которой был пойман HIGH
volatile uint32_t SOS_EndPulse   = 0;        //  Метка времени, на которой был пойман LOW
volatile uint8_t  SOS_CountPulse = 0;        //  Количество считанных фронтов импульсов
uint32_t pulse_duration          = 0;        //  Длина измеряемого периода
byte SOS_tmp                     = false;    //  Временный флаг индикации режима SOS 

//  Проверка корректности длины импульса
boolean checkLenPreamblePulse(uint32_t p_duration)
{
    if (p_duration >= (PREAMBLE_HIGH - PRECISION) && (p_duration <= (PREAMBLE_HIGH + PRECISION)))
    {
        return true;
    }
    return false;
}



// Функция обработки прерываний
void SOS_Interrupt()                        // 
{
    Settings.set_checkDeviceOff(1);                           // Установить флаг режима отключения устройства
  
    // Фиксирум/обновляем время изменения импульса

    uint32_t cur_timestamp = micros();                        //  Метка времени
    uint8_t  cur_status    = digitalRead(HL2_L_SOS);          //  Тип импульса
 
    // Начало по восхождению импульса
    if ((SOS_CountPulse == 0) && (cur_status == HIGH))
    {
        SOS_StartPulse = cur_timestamp;
        SOS_CountPulse++;
    }
     

    // Анализ по спаду
    if ((cur_status == LOW)&&(SOS_CountPulse ==1))
    {
        SOS_EndPulse = cur_timestamp;                      //  Метка времени, на которой был пойман LOW
        pulse_duration = SOS_EndPulse - SOS_StartPulse;    //  Длина первого импульса SOS

        if (checkLenPreamblePulse(pulse_duration))         // Проверяем длину HIGH импульса
        {
           SOS_CountPulse++;
           #ifdef ONE_IMPULSE_SOS
             SOS_Listening = true;                        //  Переменная - флаг получения пакета SOS
           #endif
        }
        else
        {
           // Не то пальто, обнуляем
           SOS_CountPulse = 0;
           SOS_Listening = false;                        //  Переменная - флаг получения пакета SOS
        }

    }

    /* Озмеряем расстояние между импульсами */

    // Фиксируем начало второго импульса
    if ((SOS_CountPulse == 2) && (cur_status == HIGH))
    {
        SOS_StartPulse = cur_timestamp;
        pulse_duration = SOS_StartPulse - SOS_EndPulse;   //  Длина первого импульса SOS

        if (checkLenPreamblePulse(pulse_duration))        // Проверяем длину HIGH импульса
        {
            SOS_CountPulse++;
        }
        else
        {
            // Не то пальто, обнуляем
            SOS_CountPulse = 0;
            SOS_Listening = false;                        //  Переменная - флаг получения пакета SOS
        }
    }

    /*  Измеряем длительность второго импульса*/
 
    if ((cur_status == LOW) && (SOS_CountPulse == 3))
    {
        SOS_EndPulse = cur_timestamp;                     //  Метка времени, на которой был пойман LOW
        pulse_duration = SOS_EndPulse - SOS_StartPulse;   //  Длина второго импульса SOS
        // Проверяем длину HIGH импульса
        if (checkLenPreamblePulse(pulse_duration))
        {
            SOS_CountPulse++;                            // Больше не фиксируем импульсы
            SOS_Listening = true;                        //  Переменная - флаг получения пакета SOS
        }
        else
        {
            // Не то пальто, обнуляем
            SOS_CountPulse = 0;
            SOS_Listening = false;                       //  Переменная - флаг получения пакета SOS
        }
    }
}




void setup() 
{

	SERIAL_TRACKER.begin(SERIAL_TRACKER_SPEED);
	SERIALINTERFACE.begin(SERIALINTERFACE_SPEED);         // Порт теста
	DEBUG_Serial.begin(SERIALINTERFACE_SPEED);            // Порт теста

    delay(1000);

	//DBGLN("\nSetup START");
    // while (!DEBUG_Serial && millis() < 2000);
    
    DBGLN("\nSetup START");

    Wire.begin();
    resetI2C();
    byte opt_addr =  Opt_address(0x23);     // Определить адрес оптического датчика BH1750

    if (opt_addr != 0)
    {
        Opt_Devices = 2;
    }
    if (opt_addr == 0)
    {
      opt_addr = Opt_address(0x44);         // Определить адрес оптического датчика  OPT3001
      if (opt_addr != 0)
      {
          Opt_Devices = 1;
      }
    }

    if (opt_addr != 0)
    {
        Settings.setOptDevice(Opt_Devices);
    }

      Settings.setup();     
      MemInit();

      tftModule.Setup();
      LCDScreen->onAction(screenAction);                     // тут просто сбрасываем таймер ничегонеделанья.

     //  Проверяем на инициализацию EEPROM
      uint8_t controlByte = MemRead(ControlByte_ADDRESS);    // Прочитать контрольный байт инициализации памяти

      if (controlByte != MEM_CONTROL_BYTE)                   // Сравнение контрольного бита с контрольным битом из памяти
      {
        MemClear();                                          // Стереть всю память
      }

     screenIdleTimer = millis();                             // Таймер переключения на главный экран
     backlightTimer  = millis();                             // Таймер отключения подсветки дисплея
    
      // выводим в UART версию прошивки
 	CommandHandler.getVER(&DEBUG_Serial);

	attachInterrupt(HL2_L_SOS, SOS_Interrupt, CHANGE);                // Контроль светодиода "SOS" по прерыванию. Прерывание вызывается при смене значения на порту, с LOW на HIGH и наоборот
    attachInterrupt(HL4_L_POINT, set_flag_POINT, RISING);             // Контроль светодиода "POINT" по прерыванию
    attachInterrupt(LED_GREEN_HL1, set_flag_LED_GREEN_HL1, RISING);   // Контроль светодиода "HL1_GREEN" по прерыванию
    attachInterrupt(GPS_GREEN_LED, set_flag_GSM_GREEN_LED, RISING);   // Контроль светодиода "GSM_GREEN" по прерыванию (прерывание вызывается только при смене значения на порту с LOW на HIGH)
    delay(500);
	//set_flag_POINT();                                               // Установить режим "POINT"
    Settings.displayViewPOINT(true);                                  // Установить флаг режима POINT
    Settings.displayViewSOS(false);                                   // Установить флаг режима SOS
    Settings.set_checkDeviceOff(0);                                   // Сбросить флаг индикации отключения устройства

    canCallYield = true;
    DBGLN("The program has started");
    #ifdef USE_WATCHDOG_TIMER
    Settings.reset_IWDG();                                            // Сброс сторожевого таймера
    #endif
}
//--------------------------------------------------------------------------------------------------------------------------------


void loop()
{
   
    static uint32_t tmr_SOS = millis();

   SOS_tmp = Settings.checkModeSOS();                                // Определение режима SOS

    if (SOS_Listening == true)
    {
        tmr_SOS = millis();
  /*    DBG("Set SOS - ");
        DBG(SOS_CountPulse);
        DBG(" - ");
        DBGLN(pulse_duration);*/
        SOS_CountPulse = 0;
        SOS_Listening = false;

        if (!SOS_tmp)
        {
            Settings.displayViewSOS(true);                            // Установить флаг режима SOS
            Settings.displayViewPOINT(false);                         // Установить флаг режима POINT
        }
    }

    if (millis() - tmr_SOS > DATA_MEASURE_SOS_OFF)
    {
        if (SOS_tmp)
        {
            Settings.displayViewSOS(false);                           // Установить флаг режима SOS
            Settings.displayViewPOINT(true);                          // Установить флаг режима POINT
        }
    }


    #ifdef USE_WATCHDOG_TIMER 
    Settings.reset_IWDG();                                            // Сброс сторожевого таймера
     #endif                                                           
    Settings.update();                                                // Проверяем состояние системы

    lcd_ON = Settings.isBacklightOn();                                // Проверить дисплей включен или нет
    int time_LCD_Led = Settings.GetTimeLedLCD();                      // Получить время отключения дисплея
    int time_MainMenu = Settings.GetTimeReturnMainMenu();             // Получить время возврата в главное меню
    bool powerUSB = Settings.checkConnectUSB();                       // Проверить с какого источника питается треккер

    if (powerUSB && !lcd_ON)
    {
  /*      DBGLN("LCD ON");*/
        Settings.displayBacklight(true);                              // При питании от USB включить дисплей
    }

    tftModule.Update();
  //  DBGLN("tftModule.Update");
  
    if (!lcd_ON)
    {
        backlightTimer = millis();
    }

    if (lcd_ON)
    {
        if (millis() - screenIdleTimer >= time_MainMenu * 1000)         // через XX секунд ничегонеделанья переключаемся на главный экран
        {
            AbstractLCDScreen* activeScreen = LCDScreen->getActiveScreen();
            if (activeScreen != MainScreen)
            {
                LCDScreen->resetCursor();
                screenIdleTimer = millis();
                Settings.setLEDmenu(false);
                LCDScreen->switchToScreen(MainScreen);
            }
        }

        // При питании от внутреннего источника, отключать подсветку дисплея через XX минут при отсутствии активности на кнопках
        if (millis() - backlightTimer > (time_LCD_Led * 1000))
        {
            if (!powerUSB)
            {
                Settings.displayBacklight(false);
            }
            else
            {
                Settings.displayBacklight(true);
            }
        }
    }
 
    // обрабатываем входящие команды с КОМ порта и Iridium
    CommandHandler.handleCommands();
}
//-------------------------------------------------------------------------------------
