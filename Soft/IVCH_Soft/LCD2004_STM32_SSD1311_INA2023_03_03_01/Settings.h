#pragma once
//--------------------------------------------------------------------------------------------------------------------------------
#include <Arduino.h>
#include "CoreButton.h"           // Обработка кнопок
#include "AT24CX.h"               // Поддержка энергонезависимой памяти
#include "Configuration_STM32.h"  // Основные настройки программы
#include <BH1750.h>               // Датчик света BH1750 
#include <ClosedCube_OPT3001.h>   // Датчик света OPT3001
#include <INA219.h>               // Монитор аккумулятора


#define OPT3001_ADDRESS 0x44

//--------------------------------------------------------------------------------------------------------------------------------
class SettingsClass
{
  public:
    SettingsClass();

   void setup();
 
  uint16_t GetTimeLedLCD();                                     // Получить время отображения дисплея
  void SetTimeLedLCD(uint16_t val);                             // Записать время отображения дисплея
  uint8_t GetLED_Power();                                       // Получить время отключения подсветки дисплея
  void SetLED_Power(uint8_t val);                               // Записать время отключения подсветки дисплея при отсутсвии активности
  bool GetClearUSB();                                           // получить флаг необходимости очистить экран при зарядке от USB
  void SetClearUSB(bool clear_USB);                             // установить флаг необходимости очистить экран при зарядке от USB

  uint16_t GetTimeAkk();                                        // Получить время работы аккумулятора
  void SetTimeAkk(uint16_t val);                                // Записать время работы аккумулятора
  uint16_t GetTimeReturnMainMenu();                             // Получить время возврата в главное меню
  void SetTimeReturnMainMenu(uint16_t val);                     // Записать время возврата в главное меню
  uint16_t GetPowerMinimumThreshold();                          // Получить минимальный порог напряжения аккумулятора
  void SetPowerMinimumThreshold(uint16_t val);                  // Записать минимальный порог напряжения аккумулятора
  uint16_t GetPowerMaximumThreshold();                          // Получить максимальный порог напряжения аккумулятора
  void SetPowerMaximumThreshold(uint16_t val);                  // Записать максимальный порог напряжения аккумулятора


  bool GetButtonRetention() { return ButtonRetention; }         // Получить флаг длительного нажатия кнопки
  void SetButtonRetention(bool val);                            // Записать флаг длительного нажатия кнопки

  void displayBacklight(bool bOn);                              // управление подсветкой экрана
  bool isBacklightOn() { return backlightFlag; }

  void setLEDmenu(bool ledOn);                                  // управление подсветкой кнопок
  bool isLEDmenuOn() { return setLEDmenuFlag; }

  void displayConnectBase(bool satOn);                         // Установить флаг подключения к базе IRIDIUM или GSM
  bool SAT_GSM_On() { return SAT_on; }

  void displayViewSOS(bool SOSOn);                            // Установить флаг режима SOS
  bool View_SOS_On() { return SOS_on; }
 
  void displayViewPOINT(bool POINTOn);                         // Установить флаг режима POINT
  bool View_POINT_On() { return POINT_on; }

  bool get_LED_GPS(); /*{ return GPS_LED_on; }*/               //  Определение состояния диода GPS
  void set_LED_GPS(bool led_on);                               //  Определение состояния диода GPS
  bool get_LED_GPS_status() { return GPS_on; }                 //  Определение состояния диода GPS
  void set_LED_GPS_status(bool led_status);                    // Определение длительного отключения светодиода GPS

  bool get_checkDeviceOff();                                   // Определение режима отключения прибора
  void set_checkDeviceOff(uint8_t device_off);                 // Определение режима отключения прибора

  void update();                                               // обновить данные
  uint16_t getPowerVoltageAkk(uint16_t pin);                   // Получить напряжение аккумулятора      

  uint16_t GetMemorySize();                                    // Получить максимальный размер применяемой памяти
  void SetMemorySize(uint16_t val);                            // Записать максимальный размер применяемой памяти

  bool getMessageDiodeBlink();
  void setMessageDiodeBlink(bool blink);
  bool getReadyDiodeGreenBlink();
  void setReadyDiodeGreenBlink(bool blink);
  bool getReadyDiodeRedBlink();
  void setReadyDiodeRedBlink(bool blink);
  bool getReadyDiodeOrangeBlink();
  void setReadyDiodeOrangeBlink(bool blink);
  bool getReadyDiodeAllBlink();
  void setReadyDiodeAllBlink(bool blink);
  bool getButtonBlock();
  void setButtonBlock(bool block);
  bool get_empty_buffer_request();
  void set_empty_buffer_request(bool buffer_request);


  //--------------------------------------------------------------------------------------------------------------------------------
 
  //uint8_t  getAllCoutMessage();                               // получить показания счетчика общего количества записей
  //void setAllCoutMessage(uint8_t count);                      // сохранить текущее состояние счетчика общего количества записей 

  uint8_t getCoutNotReadMessage();                              // получить показания счетчика не подтвержденного количества записей
  void setCoutNotReadMessage(uint8_t count);                    // сохранить состояние счетчика не подтвержденного количества записей 
 
  uint8_t  getCurrentCountMessage();
  void setCurrentCountMessage(uint8_t count_cur);

  uint8_t getLED_Brightness();                                  // настройка подсветки кнопок
  void setLED_Brightness(uint8_t brightness);

  uint16_t  getLuxOptModule();                                   // настройка датчика света для подсветки кнопок
  void setLuxOptModule(uint16_t val_lux);

  uint8_t  getFlippingCountMessage();
  void setFlippingCountMessage(uint8_t count_cur);

  bool getNewMessageFlag();
  void setNewMessageFlag(bool new_flag);

  bool getCompletion_flight();
  void setCompletion_flight(bool new_flag);

  bool checkConnectUSB();                                     // Определение подключения кабеля к разъему USB
  void setConnectUSB(bool USB_level);                         // Установить флаг подключения к разъему USB

  bool checkConnectBase();                                    // Определение сеанса связи с базой
  bool checkModeSOS();                                        // Определение режима SOS
  bool checkModePOINT();                                      // Определение режима POINT

  byte getOptDevice();
  byte setOptDevice(byte device);
  float lux = 0;
  bool getledConnectBase();                                   // Определение сеанса связи с базой (для индикации диодом "Готов")
  void setLedConnectBase(bool satOn);                         // Установить флаг подключения к базе IRIDIUM (для индикации диодом "Готов")
  void reset_IWDG();


  //*****************************************************************

  private:

  Button SAT_Button;
  Button LED_SOS_Button;
  byte SOS_button_tmp;
 
  void test_Led();
  int ledState = HIGH;                 // ledState used to set the LED
  int ledStateMess = HIGH;             // ledState used to set the LED
 
  void reset_LCD();                    // 

  void MX_IWDG_Init();                 // 
 
  /* Определение применяемого датчика света */
  ClosedCube_OPT3001 lightMeterOPT3001;
 /* void configureSensor();
  void printResult(String text, OPT3001 result);
  void printError(String text, OPT3001_ErrorCode error);*/
   BH1750 lightMeterBH1750;

  bool LEFT_RIGHT_Button_pressed = false;      
  bool canUseBlinker;
  bool BlinkerReadyGreen = false;
  bool BlinkerReadyRed = false;
  bool BlinkerReadyOrange = false;
  bool BlinkerReadyAll;
  bool canUseBlinkerMessage;
  bool backlightFlag;
  bool setLEDmenuFlag;
  bool lcd_ON_Flag = false;
  int index_val = 0;
  int voltageAkk1 = 0;
  bool start_meass = false;
  bool SAT_on = false;
  bool SOS_on = false;
  bool POINT_on = false;
  bool empty_buffer = false;
  bool USB_on = false;
  bool USB_on_tmp = false;
  bool GPS_on = false;
  bool GPS_on_tmp = false;
  bool GPS_LED_on = false;
  bool BlockButton = false;

  char msg_response[60];                                  // Строка для блока для ответных сообщений 
  bool ButtonRetention = false;
  bool flag = false;
  byte num_opt_device = 0;
  byte num_opt_deviceN = 0;

  bool array_countMax = false;
  int sum = 0;
  uint8_t array_count = 0;
  uint8_t array_size = 50;
  int dimension_array[50];
  uint8_t led_powerOff = 0;
  bool USB_LCD_clear = false;

  bool HL4_L_POINT_on = false;
  bool HL2_L_SOS_on = false;
  bool LED_GREEN_HL1_on = false;
  bool GPS_GREEN_LED_on = false;
  bool device_off = false;
  bool connect_base = false;         // признак подключения к базе (для управления индикатором "Готов")

    
};
//--------------------------------------------------------------------------------------------------------------------------------
extern SettingsClass Settings;
