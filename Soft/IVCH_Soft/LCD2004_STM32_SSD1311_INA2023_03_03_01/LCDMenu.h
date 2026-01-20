#pragma once
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
#include "Configuration_STM32.h"  // Основные настройки программы
#include "TinyVector.h"
#include "CoreButton.h"
#include <stdio.h>
#include <string.h> /* strlen */
#include <stdlib.h> /* atof */
#include <SSD1311.h>

#define LCD_Class SSD1311               // класс поддержки LCD 2004

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDMenu;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TickerClass Подпрограмма многократного ввода данных при удержании кнопки
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct ITickHandler
{
  virtual void onTick() = 0;
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class TickerClass
{
  public:
    TickerClass();
    ~TickerClass();

   void setIntervals(uint16_t beforeStartTickInterval,uint16_t tickInterval);
   void start(ITickHandler* h);
   void stop();
   void tick();

private:

  uint16_t beforeStartTickInterval;  // Переменная предназначена для хранения интервала, по истечении которого начнется автоматическое нажатие кнопки
  uint16_t tickInterval;             // Переменная предназначена для хранения интервала повторения нажатия кнопки

  uint32_t timer;                    // Переменная предназначена отсчета времени
  bool started, waitBefore;          // флаги 

  ITickHandler* handler;             // 
  
};
extern TickerClass Ticker;

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// абстрактный класс экрана для LCD
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class AbstractLCDScreen
{
  public:

    virtual void setup(LCDMenu* menuManager) = 0;
    virtual void update(LCDMenu* menuManager) = 0;
    virtual void draw(LCDMenu* menuManager) = 0;
    virtual void onActivate(LCDMenu* menuManager){}
    virtual void onButtonPressed(LCDMenu* menuManager,int buttonID) {}
    virtual void onButtonReleased(LCDMenu* menuManager,int buttonID) {}
	virtual void onButtonisRetention(LCDMenu* menuManager, int buttonID) {}
	virtual void onButtonisDoubleClicked(LCDMenu* menuManager, int buttonID) {}
  
    AbstractLCDScreen();
    virtual ~AbstractLCDScreen();
};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с экраном
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef void (*OnScreenAction)(AbstractLCDScreen* screen);
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDMainScreen : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDMainScreen();
	~LCDMainScreen();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onButtonisRetention(LCDMenu* menuManager, int buttonID);
	void onTick();
	void drawVoltage(LCDMenu* menuManager);
	void battery_charge(LCDMenu* menuManager);
	void drawDeviceOff(LCDMenu* menuManager);


private:
	void blinkLedPower(LCDMenu* menuManager, uint8_t val_led);
	void drawMessage(LCDMenu* menuManager, int count_message, int count_view);
	void drawPOINT(LCDMenu* menuManager);

	bool powerUSB = false;
	bool powerUSB_tmp = false;
	bool clear_on = false;

	bool wiev_SOS_on = false;
	bool wiev_SOS_on_tmp = true;

	/* переменные  для работы с кнопками */
	//int tickerButton;
	//int pressed_button;
	int released_button;
	int pressed_button_Retention;
	
	bool isActive;
	int PowerAkk = 0;                  // Контроль источника питания +3.7в
	uint8_t val = 0;
	bool power_off = false;
	bool flag_buffer = false;           // флаг буффера трекера
	int count_connect = 0;              // счетчик сеансов соединение с Iridium
	byte count_confirmation = 0;

	int TimeAkk = 0;                     // Фиксированное время работы от аккумулятора
	uint8_t val_time = 0;                //
	uint16_t operating_voltage_range =0 ;// диапазон рабочего напряжения

	uint8_t count_message;               // номер текущего сообщения 
	uint8_t flipping_count_message;      // номер листания в памяти
	uint8_t View_flipping_count_message; // Номер листания для отображения позиции просмотра относительно пришедшего сообщения

	uint8_t  confirmation_OK;

	char msg[Number_of_bytes_block] = "";
	char time_msg[Number_of_bytes_time] = "";

	uint8_t symb_Akk0 = 0;
	uint8_t symb_Akk25 = 1;
	uint8_t symb_Akk50 = 2;
	uint8_t symb_Akk75 = 3;
	uint8_t symb_Akk100 = 4;
	uint8_t symb_Mail1 = 5;
	uint8_t symb_Mail2 = 6;

	uint8_t countUSB = 0;
	bool LEFT_RIGHT_Button_pressed = false;   // Признак нажатия кнопки листания
	bool lcd_ON_Flag = false;
	bool POINT_on = false;                    // Флаг режима отключения по сигналу POINT

	Vector<Button*> hardwareButtons;

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern LCDMainScreen* MainScreen;

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDMenuScreen   Подпрограмма реализации сервисного меню
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDMenuScreen : public AbstractLCDScreen
{
public:

  LCDMenuScreen();
  ~LCDMenuScreen();

  void setup(LCDMenu* menuManager);
  void update(LCDMenu* menuManager);
  void draw(LCDMenu* menuManager);
  void onActivate(LCDMenu* menuManager);
  void onButtonPressed(LCDMenu* menuManager, int buttonID);
  void onButtonReleased(LCDMenu* menuManager, int buttonID);
  void onButtonisRetention(LCDMenu* menuManager, int buttonID);
 
private:

  int pressed_button;
  int released_button;
  int count_pressed_button;

  
  //                        У   Д   А   Л   Е   Н   И   Е      С   О   О   Б   Щ   Е   Н   И   Й
  //char   DelMassage[18] = {147,132,128,139,133,141,136,133,32,145,142,142,129,153,133,141,136,137};
  //                              П   О   Д   Т   В   Е   Р   Ж   Д   Е   Н   И   Я      О   Т   П   Р   А   В   Л   Е   Н   Ы      В      Б   У   Ф   Е   Р      Т   Р   Е   К   Е   Р   А
 // char   ConfirmationSent[40] = {143,142,132,146,130,133,144,134,132,133,141,136,159,32,142,146,143,144,128,130,139,133,141,155,32,130,32,129,147,148,133,144,32,146,144,133,138,133,144,128 };
  
};


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDClearMessageMenu  Подпрограмма очистки сообщений. Настройки программы не изменяются
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDClearMessageMenu : public AbstractLCDScreen
{
public:

	LCDClearMessageMenu();
	~LCDClearMessageMenu();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void ClearMessageM(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);

private:

	int pressed_button;
	int count_pressed_button;
	//int released_button;

	 //                      У   Д   А   Л   И   Т   Ь      С   О   О   Б   Щ   Е   Н   И   Я
	//char DelMassage[17] = { 147,132,128,139,136,146,156,32,145,142,142,129,153,133,141,136,159 };
	//                           В   Ы   Х   О   Д         У   Д   А   Л   И   Т   Ь 
	//char YesOkNo[18] = { '<',32,130,155,149,142,132,32,32,147,132,128,139,136,146,156,32,'>' };

};


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDFactorySettingMenu  Подпрограмма полной очистки данных и установка настроек по умолчанию (заводские настройки).
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDFactorySettingMenu : public AbstractLCDScreen
{
public:

	LCDFactorySettingMenu();
	~LCDFactorySettingMenu();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	//void onButtonReleased(LCDMenu* menuManager, int buttonID);

private:

	int pressed_button;
	int count_pressed_button;

};


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDViewVersion   Подпрограмма вывода на дисплей текущей версии программы
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDViewVersion : public AbstractLCDScreen
{
public:

	LCDViewVersion();
	~LCDViewVersion();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	//void onButtonReleased(LCDMenu* menuManager, int buttonID);

private:

	int pressed_button;
	//int released_button;
};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetLEDbrightness   Подпрограмма настройки яркости подсветки кнопок трекера на верхней панели
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDSetLEDbrightness : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDSetLEDbrightness();
	~LCDSetLEDbrightness();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onTick();

private:
	void incFactorLED(int val);
	char msgNum[3] = "";
	int tickerButton;
	int pressed_button;
	int released_button;
	int LED_Brightness;
	int LED_Brightness_tmp;

};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetOPTbrightness    Подпрограмма настройки порога чуствительности датчика света. Предназначен для автоматического включения подсветки кнопок при низкой освещенности 
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDSetOPTbrightness : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDSetOPTbrightness();
	~LCDSetOPTbrightness();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onTick();
	void inc_OPT_LUX(int val);

private:

	char msgNum[3] = "";
	int tickerButton;
	int pressed_button;
	int released_button;
	int OPT_Brightness;
	int OPT_Brightness_tmp;

};


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeLedLCDOff    Подпрограмма настройки времени индикации (подсветки) дисплея LCD
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDSetTimeLedLCDOff : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDSetTimeLedLCDOff();
	~LCDSetTimeLedLCDOff();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onTick();

private:

	void incTimeLCD(int val);

	int tickerButton;
	int pressed_button;
	int released_button;
	int timeLCD;
	int timeLCD_tmp;

};
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeAkkOff        Подпрограмма установки максимального времени работы прибора от питания аккумулятора
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class LCDSetTimeAkkOff : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDSetTimeAkkOff();
	~LCDSetTimeAkkOff();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onTick();

private:

	void incAkk(int val);

	int tickerButton;
	int pressed_button;
	int released_button;
	int timeAkk;
	int timeAkk_tmp;

};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeMainMenu   Подпрограмма настройки времени автоматического возврата в основной экран из других программ
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDSetTimeMainMenu : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDSetTimeMainMenu();
	~LCDSetTimeMainMenu();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onTick();

private:

	void incTimeMain(int val);

	int tickerButton;
	int pressed_button;
	int released_button;
	int timeMainMenu;
	int timeMainMenu_tmp;

};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDConfirmationsSent          Подпрограмма просмотра переданных подтверждений о прочтении полученных сообщений
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class LCDConfirmationsSent : public AbstractLCDScreen, public ITickHandler
{
public:

	LCDConfirmationsSent();
	~LCDConfirmationsSent();

	void setup(LCDMenu* menuManager);
	void update(LCDMenu* menuManager);
	void draw(LCDMenu* menuManager);
	void onActivate(LCDMenu* menuManager);
	void onButtonReleased(LCDMenu* menuManager, int buttonID);
	void onButtonPressed(LCDMenu* menuManager, int buttonID);
	void onTick();

private:

	void incConfirmationstAdrr(int val);
	char msgConf[Number_of_bytes_confirmation] = "";
	int tickerButton;
	int pressed_button;
	int released_button;
	int ConfirmationstCount = 0;
	int ConfirmationstCount_tmp = 0;

};

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct
{
  const char* screenName;
  AbstractLCDScreen* screen;
  
} LCDScreenInfo;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef Vector<LCDScreenInfo> LCDScreensList; // список экранов
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// класс-менеджер работы с LCD
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDMenu  Основная программа вывода информации на дисплей. Выполняет обработку нажатия кнопок и передачи управления на запрошенный экран
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
class LCDMenu
{

public:
  LCDMenu();

  void setup();
  void update();
  void clearLCD();

  void switchToScreen(const char* screenName);
  void switchToScreen(AbstractLCDScreen* to);

  AbstractLCDScreen* getScreen(const char* screenName);
  AbstractLCDScreen* getActiveScreen();
  void onAction(OnScreenAction handler) { on_action = handler; }
  
  LCD_Class* getDC() { return lcdDC; };
 
  void resetIdleTimer();
  void resetCursor();

  void onButtonPressed(int button);
  void onButtonReleased(int button);
  void onButtonisRetention(int button);
 
private:

	LCDScreensList screens;
	LCD_Class* lcdDC;

	int currentScreenIndex;
  
	AbstractLCDScreen* switchTo;
	int switchToIndex;

	OnScreenAction on_action;

	unsigned long idleTimer;
 
	bool LEFT_RIGHT_Button_pressed = false;
	bool button_ret_Flag = false;

	Vector<Button*> hardwareButtons;
  
};
extern LCDMenu* LCDScreen;

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
