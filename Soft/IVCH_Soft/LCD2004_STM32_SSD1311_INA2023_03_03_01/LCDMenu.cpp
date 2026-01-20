#include "LCDMenu.h"
#include "Settings.h"
#include "Memory.h"
#include "CoreButton.h"
#include "Settings.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractLCDScreen::AbstractLCDScreen()
{ 

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractLCDScreen::~AbstractLCDScreen()
{ 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDMenu  Основная программа вывода информации на дисплей. Выполняет обработку нажатия кнопок и передачи управления на запрошенный экран
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDMenu* LCDScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDMenu::LCDMenu()
{
  LCDScreen = this;
  currentScreenIndex = -1;
  switchTo = NULL;
  switchToIndex = -1;
  lcdDC = NULL;
  on_action = NULL;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::setup()
{
  
	/*  Настройка LCD дисплея LCD2004, Драйвер индикатора SSD1311 */
	lcdDC = new SSD1311();

	lcdDC->sendCommand(0x08);          //Display off	
	lcdDC->sendCommand(0x2A);          //Function Set 00101010 ==0x2A:N=1,BE=0,RE =1,REV=0

	lcdDC->sendCommand(0x71);	       //function selection A	
	lcdDC->sendCommand(0x4C);  	       //Internal VDD Regulator ON for 5V I/O Application
	//lcdDC->sendCommand(0x00);	        //Internal VDD Regulator OFF for Low Voltage I/O Application

	lcdDC->sendCommand(0x72);	       //Function Selection B
	lcdDC->sendData(0x00);

	lcdDC->sendCommand(0x79);	       //OLED characterization ON

	lcdDC->sendCommand(0x81);          //Set Contrast *****NON Default
	lcdDC->sendCommand(0xCF);

	lcdDC->sendCommand(0xD5);          //Set display clock divide Ratio
	lcdDC->sendCommand(0x10);          //default


	lcdDC->sendCommand(0xD9);          //Set phase Length***************** Non Default
	lcdDC->sendCommand(0x78);

	lcdDC->sendCommand(0xDA);          //Set SEG pin Hardware Config.	   (POR)
	lcdDC->sendCommand(0x10);	       //Default

	lcdDC->sendCommand(0xDB);          //Set VcomH Deselect Level		 *******************limit
	lcdDC->sendCommand(0x40);	       //maximum  Vcomh

	lcdDC->sendCommand(0xDC);          //Function Selection C (GPIO output High)
	lcdDC->sendCommand(0x03);          //Set VSL & GPIO
	delay(10);

	lcdDC->sendCommand(0x78);          //OLED characterization	OFF	SD=0
	lcdDC->sendCommand(0x06);          //Entry Mode Set(RE=1) BDC=1,BDS=0
	lcdDC->sendCommand(0x09);	       //Extended function Set 00001001
	lcdDC->sendCommand(0x28);	       //Function Set RE = 0,SD=0,IS = 0

	lcdDC->sendCommand(0x01);          //clear display
	lcdDC->sendCommand(0x02);          //Return home
	lcdDC->sendCommand(0x06);	       //entry mode Set  I/D=1,S=0
	lcdDC->sendCommand(0x02); 

	lcdDC->sendCommand(0x09);         // Extended function Set 00001001
	lcdDC->sendCommand(0x2A);	      // function set = >Extended command
	lcdDC->sendCommand(0x72);	      //select font table
	lcdDC->sendData(0x04);            //font D
	lcdDC->sendCommand(0x28);	      //function set = >exist extend command
	lcdDC->sendCommand(0x01);	      //clear display
	//lcdDC->sendCommand(0x0C);	      //display on

	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();           // Сброс сторожевого таймера
    #endif 

	delay(5);

	lcdDC->clear();                             // Стереть экран для вывода новой информации
	lcdDC->setCursor(0, 0);                     // Установить курсор в начальную позицию на зкране
	lcdDC->powerMode(SSD1311_LCD_ON);           // Включить дисплей
	delay(200);
	//Settings.displayBacklight(true);          // включаем подсветку
	
	resetIdleTimer();

  // добавляем служебные экраны

  LCDScreenInfo mbscrif;
   
  //LCDMainScreen
  mbscrif.screen = new LCDMainScreen();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "MAIN";
  screens.push_back(mbscrif);
  
  //LCDMenuScreen
  mbscrif.screen = new LCDMenuScreen();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "MENU";
  screens.push_back(mbscrif);

   //LCDSetTimeLedLCDOff
  mbscrif.screen = new LCDSetTimeLedLCDOff();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetTimeLedLCDOff";
  screens.push_back(mbscrif);
    
   //LCDSetTimeAkkOff
  mbscrif.screen = new LCDSetTimeAkkOff();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetTimeAkkOff";
  screens.push_back(mbscrif);

   //LCDSetTimeMainMenu
  mbscrif.screen = new LCDSetTimeMainMenu();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetTimeMainMenu";
  screens.push_back(mbscrif);

  //LCDClearMessageMenu
  mbscrif.screen = new LCDClearMessageMenu();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "ClearMessage";
  screens.push_back(mbscrif);

  //LCDSetLEDbrightness
  mbscrif.screen = new LCDSetLEDbrightness();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetLEDbrightness";
  screens.push_back(mbscrif);

  //LCDSetOPTbrightness
  mbscrif.screen = new LCDSetOPTbrightness();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "SetOPTbrightness";
  screens.push_back(mbscrif);

  //LCDViewVersion
  mbscrif.screen = new LCDViewVersion();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "ViewVersion";
  screens.push_back(mbscrif);

  //LCDFactorySettingMenu
  mbscrif.screen = new LCDFactorySettingMenu();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "FactorySettings";
  screens.push_back(mbscrif);

  //LCDConfirmationsSent
  mbscrif.screen = new LCDConfirmationsSent();
  mbscrif.screen->setup(this);
  mbscrif.screenName = "ConfirmationsSent";
  screens.push_back(mbscrif);

  //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// добавляем кнопки
Button* btn = new Button; 

// кнопка "POINT" 
btn->begin(BUTTON_POINT, true, SEND_POINT_TIME_ON);
hardwareButtons.push_back(btn);

// кнопка BUTTON_LEFT
btn = new Button;
btn->begin(BUTTON_LEFT, true, SERVISE_MENU_TIME_ON);
hardwareButtons.push_back(btn);

// кнопка BUTTON_RIGHT
btn = new Button;
btn->begin(BUTTON_RIGHT, true, SERVISE_MENU_TIME_ON); 
hardwareButtons.push_back(btn);

lcdDC->clear();                             // Стереть экран для вывода новой информации
lcdDC->setCursor(0, 0);                     // Установить курсор в начальную позицию на зкране

//for (int i = 0; i < sizeof(messageStart); i++)
//{
//	lcdDC->sendData(messageStart[i]);
//}

Settings.displayBacklight(true);            //  Включить подсветку дисплея (подачу питания на преобразователь 12 вольт)
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::onButtonPressed(int button)
{
	                             
  if(currentScreenIndex == -1)
    return;
 // Settings.displayBacklight(true);            // включаем подсветку
  resetIdleTimer();
  LCDScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->onButtonPressed(this, button);
  DBGLN("**LCDMenu onButtonPressed");		                               // Проверить 
  Settings.SetClearUSB(true);                                              // установить флаг необходимости очистить экран при зарядке от USB

  if(on_action != NULL)
  {
    on_action(currentScreenInfo->screen);
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::onButtonReleased(int button)
{
  if(currentScreenIndex == -1)
    return;

 // Settings.displayBacklight(true);                                        // включаем подсветку
  resetIdleTimer();

  LCDScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->onButtonReleased(this, button);
  DBGLN("LCDMenu::onButtonReleased");		                              // Проверить записанное сообщение. 
  Settings.SetClearUSB(true);                                             // установить флаг необходимости очистить экран при зарядке от USB
}

void LCDMenu::onButtonisRetention(int button)
{
	if (currentScreenIndex == -1)
		return;
	Settings.displayBacklight(true);                                        // включаем подсветку
	//resetIdleTimer();
	LCDScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
	currentScreenInfo->screen->onButtonisRetention(this, button);
	DBGLN("LCDMenu::onButtonisRetention");		                            // Проверить записанное сообщение. 
	Settings.SetClearUSB(true);                                             // установить флаг необходимости очистить экран при зарядке от USB
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::update()
{
	if (!lcdDC)
	{
		return;
	}
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера 
    #endif
	if (currentScreenIndex == -1 && !switchTo)                            // ни разу не рисовали ещё ничего, исправляемся
	{
		switchToScreen("MAIN");
	}

	if (switchTo != NULL)
	{
		yield();
		currentScreenIndex = switchToIndex;
		switchTo->onActivate(this);
		switchTo->update(this);
		yield();
		switchTo->draw(this);
		yield();
		//resetIdleTimer();       // сбрасываем таймер ничегонеделанья

		switchTo = NULL;
		switchToIndex = -1;
		return;
	}

	bool flight = Settings.getButtonBlock();

	if (!flight)               // Если кнопки не заблокированы при завершении полета.
	{
		// обновляем кнопки
		for (size_t i = 0; i < hardwareButtons.size(); i++)
		{
			hardwareButtons[i]->update();
		}

		// проверяем состояние кнопок
		for (size_t i = 0; i < hardwareButtons.size(); i++)
		{

			if (hardwareButtons[i]->isPressed())
			{
				// кликнута кнопка на пине pin
				uint8_t pin = hardwareButtons[i]->pinNumber();
				// тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата"
				onButtonPressed(pin);
				// подробнее по состояниям кнопки см. CoreButton.h
			}

			else if (hardwareButtons[i]->isClicked())
			{
				// кликнута кнопка на пине pin
				uint8_t pin = hardwareButtons[i]->pinNumber();

				// тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
				button_ret_Flag = Settings.GetButtonRetention(); // проверить нет ли длительного нажаимя кнопок isRetention()

				if (!button_ret_Flag)
				{
					onButtonReleased(pin);
				}
				else
				{
					Settings.SetButtonRetention(false);
				}
			}

			if (hardwareButtons[i]->isRetention())
			{
				// кликнута кнопка на пине pin
				uint8_t pin = hardwareButtons[i]->pinNumber();

				// тут можно вызывать событие для дочернего экрана, например, событие "кнопка нажата и отпущена"
				Settings.SetButtonRetention(true);     // установить флаг длительного нажатия кнопки
				onButtonisRetention(pin);
			}
		}
	}


	//// Отключаем подсветку дисплея при длительном бездействии
	//bool TFT_ON = Settings.isBacklightOn();                                // Проверить дисплей включен или нет
	//int timeLCD1 = Settings.GetTimeLedLCD();

	//if (TFT_ON)
	//{
	//	if (millis() - idleTimer > timeLCD1)
	//	{
	//		Settings.displayBacklight(false);                              // управление подсветкой экрана
	//	}
	//}
	//else
	//{
	//	/* если кнопка нажата - включить дисплей */

	////	// LCD currently off, check the touch on screen
	////	if (tftTouch->dataAvailable())
	////	{
	////		tftTouch->read();
	////		while (tftTouch->dataAvailable())
	////		{
	////			yield();
	////		}
	////		display_backlight_On();
	////		resetIdleTimer();
	////		flags.isLCDOn = true;
	////	}
	//}




  // обновляем текущий экран
  LCDScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
  currentScreenInfo->screen->update(this);
  yield();
  
  
}
void LCDMenu::clearLCD()
{
	if (!lcdDC)
	{
		return;
	}
	resetIdleTimer();
	lcdDC->clear();                             // Стереть экран для вывода новой информации
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractLCDScreen* LCDMenu::getScreen(const char* screenName)
{
  for(size_t i=0;i<screens.size();i++)
  {
    LCDScreenInfo* si = &(screens[i]);
    if(!strcmp(si->screenName,screenName))
    {
      return si->screen;
    }
  }

  return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::switchToScreen(AbstractLCDScreen* to)
{
	if(!lcdDC)
	{
		return;
	}
   // переключаемся на запрошенный экран
  for(size_t i=0;i<screens.size();i++)
  {
    LCDScreenInfo* si = &(screens[i]);
    if(si->screen == to)
    {
      switchTo = si->screen;
      switchToIndex = i;
      break;

    }
  } 
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::switchToScreen(const char* screenName)
{
	if(!lcdDC)
	{
		return;
	}
  
  // переключаемся на запрошенный экран
  for(size_t i=0;i<screens.size();i++)
  {
    LCDScreenInfo* si = &(screens[i]);
    if(!strcmp(si->screenName,screenName))
    {
      switchTo = si->screen;
      switchToIndex = i;
      break;

    }
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
AbstractLCDScreen* LCDMenu::getActiveScreen()
{
  if(currentScreenIndex > -1 && screens.size())
  {
    LCDScreenInfo* currentScreenInfo = &(screens[currentScreenIndex]);
     return (currentScreenInfo->screen);
  }  
  
  return NULL;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenu::resetIdleTimer()
{
  //idleTimer = millis();

}

void LCDMenu::resetCursor()
{
	if (!lcdDC)
	{
		return;
	}

	lcdDC->powerMode(SSD1311_LCD_OFF);
	lcdDC->cursor_on = false;                      // 
	lcdDC->cursor_blinking = false;                      // 
	lcdDC->cursor_direction = SSD1311_DIRECTION_RIGHT;
	lcdDC->BDC = false;
	lcdDC->setEntryMode();
	lcdDC->powerMode(SSD1311_LCD_ON);
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDMainScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LCDMainScreen* MainScreen = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDMainScreen::LCDMainScreen()
{
	MainScreen = this;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDMainScreen::~LCDMainScreen()
{
	//tickerButton = -1;
	//pressed_button = -1;
	pressed_button_Retention = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::onActivate(LCDMenu* menuManager)
{
	if (!menuManager->getDC())
	{
		return;
	}

	released_button = -1;
	pressed_button_Retention = -1;
	TimeAkk = Settings.GetTimeAkk();
	wiev_SOS_on = false;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::setup(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	char symbolAkk0[] =
	{
		B01001110,// ░███░
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01011111,// █████
	};

	char symbolAkk25[] =
	{
		B01001110,// ░███░
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01011111,// █████
		B01011111,// █████
	};

	char symbolAkk50[] =
	{
		B01001110,// ░███░
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01010001,// █░░░█
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
	};

	char symbolAkk75[] =
	{
		B01001110,// ░███░
		B01010001,// █░░░█
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
	};

	char symbolAkk100[] =
	{
		B01001110,// ░███░
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
		B01011111,// █████
	};

	char symbolMail1[] =
	{
		B01011111,// █████
		B01011000,// ██░░░
		B01010100,// █░█░░
		B01010010,// █░░█░
		B01010001,// █░░░█
		B01010000,// █░░░░
		B01010000,// █░░░░
		B01011111,// █████
	};

	char symbolMail2[] =
	{
		B01011111,// █████
		B01000011,// ░░░██
		B01000101,// ░░█░█
		B01001001,// ░█░░█
		B01010001,// █░░░█
		B01000001,// ░░░░█
		B01000001,// ░░░░█
		B01011111,// █████
	};



	dc->selectRomRam(SSD1311_ROM_A, SSD1311_CGRAM_1);

	dc->writeRamSymbol(symbolAkk0, symb_Akk0);
	dc->writeRamSymbol(symbolAkk25, symb_Akk25);
	dc->writeRamSymbol(symbolAkk50, symb_Akk50);
	dc->writeRamSymbol(symbolAkk75, symb_Akk75);
	dc->writeRamSymbol(symbolAkk100, symb_Akk100);
	dc->writeRamSymbol(symbolMail1, symb_Mail1);
	dc->writeRamSymbol(symbolMail2, symb_Mail2);


	/*   Переключить на русскую кодовую таблицу */
	dc->sendCommand(0x2A);	             // function set = >Extended command
	dc->sendCommand(0x72);	             //select font table
	dc->sendData(0x04);                  //font D
	dc->sendCommand(0x28);	             //function set = >exist extend command
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::onButtonPressed(LCDMenu* menuManager, int buttonID)
{
	//pressed_button = buttonID;
	//DBGLN("LCDMainScreen::onButtonPressed..");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::onButtonReleased(LCDMenu* menuManager, int buttonID)
{
	released_button = buttonID;
	//DBGLN("LCDMainScreen::onButtonReleased..");
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::onButtonisRetention(LCDMenu* menuManager, int buttonID)
{
	//DBGLN("LCDMainScreen::onButtonisRetention..");
	pressed_button_Retention = buttonID;
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::onTick()
{

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::update(LCDMenu* menuManager)
{

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	// **********************************************************************************
	/*
	- Проверить есть ли новое сообщение
	- Если есть, приступить к обработке
	- Сбросить флаг прихода нового сообщения
	- получить порядковый номер нового сообщения
	- присвоить счетчику просмотра "0"
	- вызвать программу отображения информации на дисплее
	*/
	#ifdef USE_WATCHDOG_TIMER
	Settings.reset_IWDG();      // Сброс сторожевого таймера 
    #endif

	powerUSB = Settings.checkConnectUSB();                                     // Проверить с какого источника питается треккер

	/* ========определение режима индикации светодиодов SOS и POINT ======== */
	/*
	
	
	
	*/

	byte mod_sos = Settings.checkModeSOS();                                       // Определение режима SOS
	byte mod_point = Settings.checkModePOINT();                                   // Определение режима POINT

//if( mod_sos) DBGLN("BUTTON_SOS_POINT");

	if (mod_sos /*&& (!power_off) && !mod_point*/)                                                   // Кнопка "Point" не должна быть нажата
	{
		/*
		Условие индикации режима SOS
		1 Должен светится индикатор SOS длительное время
		2 Это не режим отключения, когда мигают все светодиоды (!power_off)
		*/

		Settings.displayViewPOINT(false);                                  // Установить флаг режима POINT

		wiev_SOS_on = true;
 
	}

	if (mod_point && (!power_off) && !mod_sos)                       // Определение режима POINT
	{
	   wiev_SOS_on = false;
       dc->setCursor(17, 2);                                                // Установить курсор в позицию на зкране
       dc->print("   ");
       wiev_SOS_on_tmp = false;
	}

	/* Вывод на дисплей состояниии индикации светодиода SOS*/
	if (wiev_SOS_on)
	{

		dc->setCursor(17, 2);                                                 // Установить курсор в позицию на зкране
		dc->print("SOS");
       if(wiev_SOS_on_tmp != wiev_SOS_on)
       {
        Settings.displayBacklight(true);                                         // выполнить включение подсветки дисплея
        wiev_SOS_on_tmp = wiev_SOS_on;
   
       }
 	}
	else
	{
	//	dc->setCursor(17, 2);                                                // Установить курсор в позицию на зкране
	//	dc->print("   ");

	}
 
	/* Проверить пришло ли новое сообщение. */
	bool new_flag = Settings.getNewMessageFlag();                             //  Получить признак нового сообщения 
	if (new_flag)                                                             // если новое сообщение
	{
		Settings.setNewMessageFlag(false);                                    // Сбросить флаг нового сообщения. Программа извещена и приступила к обработке нового сообщения.
		count_message = Settings.getCurrentCountMessage();                    // получить номер текущего сообщения 
		flipping_count_message = count_message;
		Settings.setFlippingCountMessage(flipping_count_message);             // Установить номер листания на позицию пришедшего сообщения
		View_flipping_count_message = 0;                                      // Номер просмотра переключить в "0"
		Settings.displayBacklight(true);                                      // выполнить включение подсветки дисплея
		drawMessage(menuManager, count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее
	}


	//********************* Вызов Меню настроек ***************************

	if (pressed_button_Retention != -1)
	{

		if (pressed_button_Retention == BUTTON_LEFT)
		{
			pressed_button_Retention = -1;
			POINT_on = false;                           // Флаг режима отключения по сигналу POINT
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button_Retention == BUTTON_RIGHT)
		{
			pressed_button_Retention = -1;
			POINT_on = false;                           // Флаг режима отключения по сигналу POINT
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button_Retention == BUTTON_POINT)   // нажата и удерживается кнопка POINT
		{
			pressed_button_Retention = -1;
			//DBGLN("BUTTON_POINT");
  
			if ((digitalRead(BUTTON_SOS) == 0) && (!power_off))              // Определили, что кнопка SOS так же нажата При работе/старте прибора признак "power_off" равен  false
			{
				dc->setCursor(16, 2);                                        // Установить курсор в позицию на зкране
				dc->print("    ");
				Settings.set_checkDeviceOff(0);                              // Сбросить прерывание по контролю индикации светодиодами
	
				int var = 0;

				while (var < 1000)                                           // Ожидаем включения всех светодиодов при отключении прибора
				{
					if ((digitalRead(LED_GREEN_HL1) == HIGH) && (digitalRead(GPS_GREEN_LED) == HIGH) && (digitalRead(HL4_L_POINT) == HIGH) && (digitalRead(HL2_L_SOS) == HIGH))
					{
						power_off = true;                                    // Установить признак отключения трекера для последующего вывывода надписи об выключениии трекера
						break;
					}

					delay(10);
					var++;
				}

				Settings.set_checkDeviceOff(0);                               // контроль режима отключения прибора.

				if (power_off)
				{
					drawDeviceOff(menuManager);
				}
			}
			else   // Определили, что это не отключение а передача сообщения о завершении полета.
			{
				/* Определяем  что это не отключение а передача сообщения о завершении полета.*/

				bool GPS_off = false;
				POINT_on = true;                         // Флаг режима отключения по сигналу POINT
				Settings.set_LED_GPS_status(false);      // Сбросить флаг длительного отключения светодиода GPS

					int var = 0;
					while (var < 5000) 
					{
						#ifdef USE_WATCHDOG_TIMER 
						  Settings.reset_IWDG();      // Сброс сторожевого таймера
						#endif
						var += 100;
						delay(100);
						GPS_off = Settings.get_LED_GPS_status();       // Ждем отключения светодиода GPS
	
						if (GPS_off)
						{
							DBG("GPS_off - ");
							DBGLN(GPS_off);
							dc->clear();                               // Стереть экран для вывода новой информации
							dc->setCursor(0, 0);                       // Установить курсор в начальную позицию на зкране
							break;
							//return;
						}
					}

				//DBGLN("drawPOINT ");
				//DBGLN(GPS_off);
				Settings.setButtonBlock(true);
				drawPOINT(menuManager);                                // Вывод сообщения  "ПЕРЕДАЧА УСПЕШНОГО ЗАВЕРШЕНИЯ ПОЛЕТА"
			}
		}
	}

	//******************** выполнение действий кнопок ******************************
	//pressed_button

	else if (released_button != -1)
	{
		if (released_button == BUTTON_POINT)
		{
			released_button = -1;
			//DBGLN("BUTTON_POINT");

			lcd_ON_Flag = Settings.isBacklightOn();                                // получить состояние дисплея, включен или нет
			Settings.setMessageDiodeBlink(false);                                  // запретить мигание светодиода "Сообщение" (красный)
			//digitalWrite(LED_RED_MESSAGE, LOW);                                  // Принудительно погасить светодиод  "Сообщение" (красный)
      //Settings.displayViewSOS(false);                                        // Установить флаг режима SOS
      //Settings.displayViewPOINT(true);                                       // Установить флаг режима POINT
      
			if (!lcd_ON_Flag)                                                      // Если подсветка дисплея выключена:
			{
				//Settings.displayBacklight(true);                                 // выполнить включение подсветки дисплея
				//LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
			}
			else                                                                   // подсветка дисплея включена  - сформировать и отправить сообщение о прочтении.
			{
				LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран

				count_message = Settings.getCurrentCountMessage();                 // получить номер текущего сообщения 
				flipping_count_message = Settings.getFlippingCountMessage();       // получить номер листания сообщения 

				/*  вычисляем адрес вызываемого сообщени сообщения. */
				unsigned int cur_adr = (flipping_count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS - Number_of_bytes_block; // вычисляем адрес вызываемого сообщени сообщения.

				/* определяем было ли отправлено подтверждение прочтения текущего сообщения или нет*/
				confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);                   // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
				uint8_t  num_receive_in_message = MemRead(cur_adr + addr_number_this_message);    // Получить номер сообщения пришедшего из центра. Листать сообщения максимально ограничиваться этим номером
				uint8_t not_read = Settings.getCoutNotReadMessage();                              // получить показания счетчика не подтвержденного количества сообщений. 
				/* проконтролируем в КОМ порту количество неподтвержденных сообщений*/
				/*DBG("not_read - ");
				DBGLN(not_read);
				DBG("confirmation_OK - ");
				DBGLN(confirmation_OK);*/
				//----------------------------------------------------------------------------------

				if (confirmation_OK == MESSAGE_NOT_CONFIRMED)                                     // Получен специальный код признака "MESSAGE_NOT_CONFIRMED" - означает что подтверждение не отправлено
				{
					char msgOK_Display[60] = "OK->";                                              // Массив для ответного сообщения 
					char msgOK_Trecker[10] = "|OK";                                               // Формирование строки для ответного сообщения 
					char msgNum[2] = "";                                                          // массив для записи номера ответного сообщения
					char msg[60] = "";                                                            // Массив для приема текстовых сообщений
					char msg_resp[60] = "";                                                       // 
					char msg_resp_tmp[60] = "";                                                   //

					itoa(num_receive_in_message, msgNum, 10);                                     // Преобразовать в строку номер сообщения пришедшего из центра

					/* формируем строку ответа для передачи на треккер */
					strcat(msgOK_Trecker, msgNum);                                                // Добавили в "|OK" номер ответного сообщения
					MemReadChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));     // Получить из памяти уже имеющуюся строку с подтверждеяниями
					strcat(msg_resp, msgOK_Trecker);                                              // Добавили к текущему ответу новый ответ. Формируем строку с несколькими ответами
					MemWriteChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));    // Сохраняем в памяти вновь сформированную строку с подтверждениями о прочтении
					MemWrite(NEW_CONFIRMATION_MESSAGE, MESSAGE_GENERATED);                         // Сохраняем в памяти признак сформированного нового сообщения
				
					/************** формируем строку ответа для вывода на экран ********************/
					MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));                // Извлечь пришедшее и записанное сообщение. 
					/*  Формируем строку о прочтении для вывода на дисплей */
					strcat(msgOK_Display, msgNum);                                                 // Прибавляем к строке "OK->"+ N (номер входящего сообщения)
					strcat(msgOK_Display, msg);                                                    // Прибавляем к строке "OK->"+ N + текст входящего сообщения
					delay(10);
					MemWriteChars(cur_adr + addr_current_message, msgOK_Display, sizeof(msgOK_Display)); // Записать в память ответное сообщение по текущему адресу для последующего контроля

					if (not_read > 0)                                                              // Если счетчик неподтвержденных сообщений больше нуля                           
					{
						not_read--;                                                                // уменишаем счетчик на один 
						Settings.setCoutNotReadMessage(not_read);                                  // и сохраняем в памяти, обновить показания счетчика не подтвержденного () количества сообщений
					}

					MemWrite(cur_adr + addr_read_NOT_TRANSMITTED, MESSAGE_ACKNOWLEDGED);           // устанавливаем флаг ("MESSAGE_ACKNOWLEDGED") о подтверждении прочтения в блок сообщения
					View_flipping_count_message = count_message - flipping_count_message;
					drawMessage(menuManager, flipping_count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее
				}
			}
		}

		// ****************  обработка нажатия кнопок ЛЕВО ПРАВО  ***************************

		else if (released_button == BUTTON_LEFT)
		{
			released_button = -1;                                  // сбросить флаг нажатой кнопки
			//DBGLN("BUTTON_LEFT");
			lcd_ON_Flag = Settings.isBacklightOn();

			if (!lcd_ON_Flag)
			{
				Settings.displayBacklight(true);                                 //Включить отображение информации на дислее
				LCDScreen->resetIdleTimer();                                     // сбросить таймер ничего неделанья
			}
			else
			{
				LEFT_RIGHT_Button_pressed = true;                                // признак нажатия кнопки листания
				LCDScreen->resetIdleTimer();                                     // сбросить таймер ничего неделанья
				Settings.setMessageDiodeBlink(false);                            // запретить мигание светодиода "Сообщение" (красный)
				//digitalWrite(LED_RED_MESSAGE, LOW);                            // Принудительно погасить светодиод  "Сообщение" (красный)

				count_message = Settings.getCurrentCountMessage();               // получить номер текущего сообщения 
				flipping_count_message = Settings.getFlippingCountMessage();     // получить номер листания сообщения 
	/*			DBGLN("BUTTON_LEFT");
				DBG("count_message ");
				DBGLN(count_message);
				DBG("flipping_count_message ");
				DBGLN(flipping_count_message);*/

				if (flipping_count_message > 1)
				{
					flipping_count_message--;
					Settings.setFlippingCountMessage(flipping_count_message);                     // сохранить номер листания сообщения 
					View_flipping_count_message = count_message - flipping_count_message;
					drawMessage(menuManager, flipping_count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее
				}
			}
		}

		if (released_button == BUTTON_RIGHT)
		{
			released_button = -1;                                                           // сбросить флаг нажатой кнопки

			lcd_ON_Flag = Settings.isBacklightOn();                                         //
			//DBGLN("BUTTON_RIGHT");

			if (!lcd_ON_Flag)                                                               // Если дисплей погашен - только включить отображение
			{
				LEFT_RIGHT_Button_pressed = true;                                           // установить признак нажатия кнопки
				Settings.displayBacklight(true);
				LCDScreen->resetIdleTimer();
				delay(100);
				count_message = Settings.getCurrentCountMessage();                          // получить номер текущего сообщения 
				flipping_count_message = Settings.getFlippingCountMessage();                // получить номер листания сообщения 
				View_flipping_count_message = count_message - flipping_count_message;
				drawMessage(menuManager, flipping_count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее

			}
			else // Дисплей отображает
			{
				LEFT_RIGHT_Button_pressed = true;                                           // установить признак нажатия кнопки
				LCDScreen->resetIdleTimer();                                                // Сбросить таймер перехода на главный экран
				Settings.setMessageDiodeBlink(false);                                       // запретить мигание светодиода "Сообщение" (красный)
				//digitalWrite(LED_RED_MESSAGE, LOW);                                         // Принудительно погасить светодиод  "Сообщение" (красный)

				count_message = Settings.getCurrentCountMessage();                          // получить номер текущего сообщения 
				flipping_count_message = Settings.getFlippingCountMessage();                // получить номер листания сообщения 

		/*		DBGLN("BUTTON_RIGHT");
				DBG("count_message ");
				DBGLN(count_message);*/
	
				flipping_count_message++;
				if (flipping_count_message > count_message)
				{
					flipping_count_message = count_message;

				}
	/*			DBG("flipping_count_message ");
				DBGLN(flipping_count_message);*/
				Settings.setFlippingCountMessage(flipping_count_message);                     // сохранить номер листания сообщения 
				View_flipping_count_message = count_message - flipping_count_message;
				drawMessage(menuManager, flipping_count_message, View_flipping_count_message); // вызвать программу отображения информации на дисплее

			}
		}
	}



	//***************************  Программа передачи строки с подтверждениями о прочтении в буфер треккера   ***************************************************/	
	
	bool gsm_sat_enable = Settings.SAT_GSM_On();   // получить признак о том что была связь с Iridium

	if (gsm_sat_enable)
	{
		char msg_resp[40] = "";
		char msgOK_Display[40] = "->";                                                     // Массив для ответного сообщения 
		MemReadChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));          // Получить из памяти строку ответного сообщения
		byte mess_generated = MemRead(NEW_CONFIRMATION_MESSAGE);                           // Получить из памяти признак сформированного нового сообщения
		
		if (mess_generated)
		{
			flag_buffer = true;                                                            // Старт отслеживания передачи подтверждений на IRIDIUM
		}
		if(flag_buffer)      
		{
			count_connect++;                                                               // Считаем количество соединений до успешной передачи текста подтверждения
		}
		
		delay(50);// 
	
		if ((strlen(msg_resp) > 0) && (mess_generated == MESSAGE_GENERATED))               // Передаем в буфер информацию только в том случае если в буфере что то есть и и есть флаг нового сообщения
		{

			SERIAL_TRACKER.println(msg_resp);                                              // Передать подтерждение о прочтении сообщения в буфер треккера
	
			/* Выводим на дисплей отправленное сообщение (для контроля отправки подтверждения) */
			strcat(msgOK_Display, msg_resp);                                               // Добавили в ответ номера ответных сообщений
			dc->clear();                                                                   // Стереть экран для вывода новой информации
			dc->setCursor(0, 1);                                                           // Установить курсор в начало экрана
			dc->print(msgOK_Display);

			/* Сохраним в памяти все строки переданных сообщений для последующего контроля */
			count_confirmation = MemRead(Current_Counter_confirmation);                // Получить номер текущего подтверждения
			unsigned int cur_adr_conf = (Number_of_bytes_confirmation * count_confirmation) + Start_confirmation_ADDRESS; //  получить  адрес текущего сообщения.
			MemWriteChars(cur_adr_conf, msgOK_Display, sizeof(msgOK_Display));              // Записать в память строку переданного сообщения
			count_confirmation++;                                                           // увеличим номер сохраненного подтверждения.
			MemWrite(Current_Counter_confirmation, count_confirmation);                     // сохраним новый номер сохраненного подтверждения.

			msg_resp[0] = 0;                                                                // Очистить строку сообщения
			//msgOK_Display[0] = 0;
			MemWriteChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));      // Записать в память пустое сообщение по текущему адресу 
			delay(100);// 
			MemReadChars(Response_message_block_ADDRESS, msg_resp, sizeof(msg_resp));       // Получить из памяти строку ответного сообщения (для проверки)
			if (strlen(msg_resp) == 0)                                                      // если сообщений нет, сбрасываем признак наличия строки подтверждения
			{
				MemWrite(NEW_CONFIRMATION_MESSAGE, MESSAGE_NOT_GENERATED);                  // Сбрасываем в памяти признак сформированного нового сообщения
			}
		}
		Settings.displayConnectBase(false);                                                 // Сбрасываем признак нового коннекта с Iridium
	}

	bool flight = Settings.getButtonBlock();

	static uint32_t tmr = millis();
	if (millis() - tmr > DATA_MEASURE_THRESHOLD)                                      // Обновляем и отображаем заряд аккумулятора 1 раз в секунду или DATA_MEASURE_THRESHOLD
	{
		tmr = millis();
		//powerUSB = Settings.checkConnectUSB();                                      // Проверить с какого источника питается треккер

		if (powerUSB)
		{
			if (powerUSB != powerUSB_tmp)
			{
				Settings.displayBacklight(true);                                     // При питании от USB включить дисплей
				powerUSB_tmp = powerUSB;
				dc->clear();                                                         // Стереть экран для вывода новой информации
			}
			battery_charge(menuManager);
		}
		else
		{
			if (!flight)
			{
				drawVoltage(menuManager);                                            // Отображение уровня заряда аккумуляторов
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::draw(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	dc->clear();                             // Стереть экран для вывода новой информации
	dc->printRus("ТРЕКЕР",7, 0);
	dc->printRus("ПЕРЕДАЧА ИНФОРМАЦИИ",1,1);
	dc->printRus("ПИЛОТУ",7,2);
	wiev_SOS_on = false;
	
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::drawMessage(LCDMenu * menuManager, int count_message, int count_view)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}
	         
	/*
	- Вычисляем адрес сообщения в памяти, применяя номер сообщения
	- Записать в массив msg сообщение, хранящееся в памяти
	- Записать в массив time_msg дату и время из сообщения, хранящееся в памяти
	- Извлекаем флаг 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
	- Получить показания счетчика не прочитанного количества сообщений
	- Получить признак нового сообщения 
	- получить номер листания сообщения для отображения на дисплее
	- Выводим на дисплей с первой позиции сообщение
	- Выводим на дисплей на центр 3 строки дату и время, полученные из сообщения.
	- Выводим на дисплей номер текущего сообщения
	- Выводим на дисплей символ, обозначающий, было передано подтверждение или нет
	- Выводим на дисплей количество не подтвержденных о прочтении сообщений
	- Если есть неподтвержденные сообщения. выводим значок почтового конверта
	
	*/

	//LCDScreen->resetIdleTimer();                                                    // сбрасываем таймер ничегонеделанья

	unsigned int cur_adr = (count_message * Number_of_bytes_block) + Start_Block_Text_ADDRESS - Number_of_bytes_block; //  получить  адрес текущего сообщения.
	MemReadChars(cur_adr + addr_current_message, msg, sizeof(msg));                 // Считать из памяти записанное сообщение.
	MemReadChars(cur_adr + addr_time_this_message, time_msg, sizeof(time_msg));     // Считать из памяти время соббщения в память по текущему адресу 
	uint8_t  confirmation_OK = MemRead(cur_adr + addr_read_NOT_TRANSMITTED);        // 1 байт - флаг передачи подтверждения "ОК". "1" подтверждение прочтения НЕ ПЕРЕДАНО
	uint8_t not_read = Settings.getCoutNotReadMessage();                            // получить показания счетчика не подтвержденного количества сообщений

		/*Вывести на дисплей сообщение*/
	dc->clear();                                                                    // Стереть экран
	dc->setCursor(0, 0);                                                            // Установить курсор в начало экрана
	dc->print(msg);                                                                 // Отобразить новое сообщение

	dc->setCursor(5, 3);                                                            // Установить курсор для вывода времени сообщения
	dc->print(time_msg);                                                            // Отобразить время сообщения

	/* отобразить состояние подтверждения прочтения сообщения*/

	char str[1];
	int cursorNum = 0;
	dc->setCursor(cursorNum, 3);                                                   // Установить курсор в начало экрана на нижней строке
	if (count_view < 10)
	{
		itoa(count_view, str, 10);                                                 // Преобразуем номер текущего сообщения в строку
		dc->print(str);                                                            // Отображаем в первой позиции номер сообщения.Выводим на дисплей номер текущего сообщения
	}
	else
	{
		dc->print("X");                                                           // Если количество больше 10, выводим символ "Х" для этономии знакомест.
	}

	if (confirmation_OK == MESSAGE_NOT_CONFIRMED)                                 // Для данного сообщения подтверждение о прочтении не передано
	{
		dc->print("*");                                                           // Выводим на дисплей символ, обозначающий, было передано подтверждение или нет
	}

	/*  Отобразить количество не прочитанных сообщений */
	if (not_read != 0)                                                           // not_read = показания счетчика не прочитанного количества сообщений
	{
		dc->setCursor(cursorNum + 2, 3);                                         // Установить курсор 
				
		if (not_read < 10)
		{
			itoa(not_read, str, 10);                                            // Записать в строку количество не подтвержденных о прочтении сообщений
			dc->print(str);                                                     // Отобразить количество не подтвержденных о прочтении сообщений
		}
		else
		{
			dc->print("X");                                                     // Если количество больше 10, выводим символ "Х" для этономии знакомест.
				
		}
				
		/*Отображаем значок конверта*/
		dc->setCursor(cursorNum + 3, 3);                                        // Установить курсор 
		dc->sendData(symb_Mail1);
		dc->sendData(symb_Mail2);
	}	
	drawVoltage(menuManager);                                                  // Отображение уровня заряда аккумуляторов
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::drawPOINT(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	dc->clear();                                            // Стереть экран для вывода новой информации
	dc->setCursor(0, 0);                                    // Установить курсор в начальную позицию на зкране
	int var = 0;

	while (var < 500)
	{
		#ifdef USE_WATCHDOG_TIMER 
			Settings.reset_IWDG();      // Сброс сторожевого таймера
		#endif
		var += 100;
		delay(100);

	}

	dc->clear();                             // Стереть экран для вывода новой информации
	dc->printRus("ПЕРЕДАЧА", 6, 0);
	dc->printRus("УСПЕШНРГО ЗАВЕРШЕНИЯ", 0, 1);
	dc->printRus("ПОЛЕТА", 7, 2);

	var = 0;
	while (var < 3000)
	{
		#ifdef USE_WATCHDOG_TIMER 
			Settings.reset_IWDG();      // Сброс сторожевого таймера
		#endif
		var += 100;
		delay(100);
	
	}

	dc->clear();
	Settings.displayBacklight(false);            // выключаем подсветку

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


void LCDMainScreen::drawDeviceOff(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	Settings.set_checkDeviceOff(0);                         // контроль режима отключения прибора.
	dc->clear();                                            // Стереть экран для вывода новой информации
	dc->setCursor(0, 0);                                    // Установить курсор в начальную позицию на зкране


	dc->clear();                             // Стереть экран для вывода новой информации
	dc->printRus("ТРЕКЕР", 6, 0);
	dc->printRus("ВЫКЛЮЧАЕТСЯ", 4, 1);

	int var = 0;
	while (var < 3000)
	{
	#ifdef USE_WATCHDOG_TIMER 
			Settings.reset_IWDG();          // Сброс сторожевого таймера
	#endif
		var += 100;
		delay(100);

	}

	dc->clear();                                            // Стереть экран для вывода новой информации
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Контроль внутреннего источника питания (аккумуляторов)
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::drawVoltage(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();
	if (!dc)
	{
		return;
	}


	char msg_timeAkk[2] = "";

	PowerAkk = Settings.getPowerVoltageAkk(POWER_BATTERY);                  // Получить напряжение источника питания(Аккумулятора)
	TimeAkk = Settings.GetTimeAkk();                                        // Получить максимальное время работы аккумулятора
	operating_voltage_range = DEFAULT_POWER_HIGH - DEFAULT_POWER_LOW;       // диапазон рабочего напряжения

	if (PowerAkk > DEFAULT_POWER_LOW)                                       // Фиксирум результат измерения напряжения аккумулятора после включения устройства
	{
		PowerAkk = max(PowerAkk, DEFAULT_POWER_LOW);                        // ограничить нижний уровень
		PowerAkk = min(PowerAkk, DEFAULT_POWER_HIGH);                       // ограничить верхний уровень

		/*Вычисляем процент заряда (для отображения графического изображения на дисплее) 
		Диапазон в единицах от 0 до 100 Процент заряда (для отображения графического
		изображения на дисплее) Диапазон в единицах от 0 до 100  от диапазона DEFAULT_POWER_LOW, DEFAULT_POWER_HIGH */
		val = map(PowerAkk, DEFAULT_POWER_LOW, DEFAULT_POWER_HIGH, 0, 100); // 

		/*Вычисляем процент заряда в часах (для отображения часов на дисплее)
		Диапазон в единицах от 0 до TimeAkk (по умолчанию до 18 часов)*/
		val_time = map(val, 0, 100, 0, TimeAkk);                            // Вычисление процентного значения ВРЕМЕНИ работы аккумулятора
	}

	dc->setCursor(17, 3);                                                   // Установить курсор 

	if (val < 10)
	{
		dc->sendData(symb_Akk0);
	}
	if ((val >= 10) && (val < 30))
	{
		dc->sendData(symb_Akk25);
	}
	if ((val >= 30) && (val < 50))
	{
		dc->sendData(symb_Akk50);
	}
	if ((val >= 50) && (val < 70))
	{
		dc->sendData(symb_Akk75);
	}
	if (val >= 70)
	{
		dc->sendData(symb_Akk100);
	}

	/*вывести на дисплей остаток времени работы от аккумулятора*/
	//val -     Процент заряда (для отображения графического изображения на дисплее)
	//TimeAkk - Фиксированное время работы от аккумулятора
  
	itoa(val_time, msg_timeAkk, 10);                       //

	dc->setCursor(18, 3);
	dc->print("  ");
	dc->setCursor(18, 3);
	dc->print(msg_timeAkk);
	
     /* Вычисление процентного значения текущего значения заряда (остаток времени )для отображения светодиодом "Готов"
	 За базу принимается  время от 0 до TimeAkk (максимальное время работы аккумулятора) относительно фактического времени
	 диапазон 0 до 100 */
	uint8_t led_POWER = map(val_time, 0, TimeAkk, 0, 100);  // Вычисление процентного значения текущего значения заряда (остаток времени )для отображения светодиодом "Готов" 
	Settings.SetLED_Power(led_POWER);                       // сохранить время отключения подсветки дисплея при отсутсвии активности, снижении питания и перехода на фиксированное время отключения

	blinkLedPower(menuManager, led_POWER);                  // Отобразить состояние питания светодиодом "ГОТОВ"


}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Контроль заряда источника питания (аккумуляторов)
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMainScreen::battery_charge(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();
	if (!dc)
	{
		return;
	}

	char msg_timeAkk[2] = "";

	PowerAkk = Settings.getPowerVoltageAkk(POWER_BATTERY);                  // Получить напряжение источника питания(Аккумулятора)
	TimeAkk = Settings.GetTimeAkk();                                        // Получить максимальное время работы аккумулятора
	operating_voltage_range = DEFAULT_POWER_HIGH - DEFAULT_POWER_LOW;       // диапазон рабочего напряжения

	if (PowerAkk > DEFAULT_POWER_LOW)                                       // Фиксирум результат измерения напряжения аккумулятора после включения устройства
	{
		PowerAkk = max(PowerAkk, DEFAULT_POWER_LOW);                        // ограничить нижний уровень
		PowerAkk = min(PowerAkk, DEFAULT_POWER_HIGH);                       // ограничить верхний уровень

		/*Вычисляем процент заряда (для отображения графического изображения на дисплее)
		Диапазон в единицах от 0 до 100 Процент заряда (для отображения графического
		изображения на дисплее) Диапазон в единицах от 0 до 100  от диапазона DEFAULT_POWER_LOW, DEFAULT_POWER_HIGH */
		val = map(PowerAkk, DEFAULT_POWER_LOW, DEFAULT_POWER_HIGH, 0, 100); // 

		/*Вычисляем процент заряда в часах (для отображения часов на дисплее)
		Диапазон в единицах от 0 до TimeAkk (по умолчанию до 18 часов)*/
		val_time = map(val, 0, 100, 0, TimeAkk);                            // Вычисление процентного значения ВРЕМЕНИ работы аккумулятора
	}

	/*вывести на дисплей остаток времени работы от аккумулятора*/
	//val -     Процент заряда (для отображения графического изображения на дисплее)
	//TimeAkk - Фиксированное время работы от аккумулятора

	itoa(val_time, msg_timeAkk, 10);

	dc->setCursor(18, 3);
	dc->print("  ");
	dc->setCursor(18, 3);
	dc->print(msg_timeAkk);

	/* Вычисление процентного значения текущего значения заряда (остаток времени )для отображения светодиодом "Готов"
	За базу принимается  время от 0 до TimeAkk (максимальное время работы аккумулятора) относительно фактического времени
	диапазон 0 до 100 */
	uint8_t led_POWER = map(val_time, 0, TimeAkk, 0, 100);     // Вычисление процентного значения текущего значения заряда (остаток времени )для отображения светодиодом "Готов" 
	Settings.SetLED_Power(led_POWER);                          // сохранить время отключения подсветки дисплея при отсутсвии активности, снижении питания и перехода на фиксированное время отключения

	//*********** Отображение заряда аккумулятора графическими символами********************

	/* Отсюда */
	static uint32_t tmr_charge = 0;                            // Счетчик для отсчета времени стабилизации измерений.

	bool USB_LCD_Clear = Settings.GetClearUSB();               // получить флаг необходимости очистить экран при зарядке от USB
	
	if(USB_LCD_Clear)
	{
		Settings.SetClearUSB(false);                           // установить флаг необходимости очистить экран при зарядке от USB
		tmr_charge = millis();
		clear_on = true;
	}

	if ((millis() - tmr_charge > 1000 * 25)&&(clear_on))      // Немного подождем до установления корректных измерений напряжения после старта
	{
		dc->clear();                                          // Стереть экран для вывода новой информации
		clear_on = false;
	}

	if (powerUSB != powerUSB_tmp)
	{
		powerUSB_tmp = powerUSB;
		//dc->clear();                                          // Стереть экран для вывода новой информации
	}

	/* и до сюда очищаем экран при зарядке от USB*/

	countUSB++;
	if (countUSB > 5)
	{
		countUSB = 0;
	}

	dc->setCursor(17, 3);                                       // Установить курсор 

	switch (countUSB)
	{
	case 1:
		dc->sendData(symb_Akk0);
		break;
	case 2:
		dc->sendData(symb_Akk25);
		break;
	case 3:
		dc->sendData(symb_Akk50);
		break;
	case 4:
		dc->sendData(symb_Akk75);
		break;
	case 5:
		dc->sendData(symb_Akk100);
		break;

	default:
		break;
	}

	blinkLedPower(menuManager, led_POWER);                             // Отобразить состояние уровня питания светодиодом "ГОТОВ"
}

void LCDMainScreen::blinkLedPower(LCDMenu* menuManager, uint8_t val_led)
{

	bool allLedBlink = Settings.getReadyDiodeAllBlink();               // получить состояние мигание светодиода "Готов" (красный и зеленый)      
	bool flight = Settings.getButtonBlock();

	if (!flight)
	{
		if (allLedBlink)
		{
			// Отобразить состояние заряда аккумулятора прерывистым свечением до установления связи с Iridium
			static uint32_t tmr_start = millis();
			if (millis() - tmr_start > 1000)                               // Немного подождем до установления корректных измерений напряжения после старта
			{
				tmr_start = millis();

				if (val_led < 15)                                          // определен минимальный уровень заряда аккумулятора
				{

					Settings.setReadyDiodeGreenBlink(false);               //  Запретить мигание светодиода "Готов" (зеленый)   
					Settings.setReadyDiodeRedBlink(true);                  //  Разрешить мигание светодиода "Готов" (красный)  
					Settings.setReadyDiodeOrangeBlink(false);              //  Запретить мигание светодиода "Готов" (оранжевый) 
				}
				else if ((val_led >= 15) && (val_led < 50))                // определен средний уровень заряда аккумулятора
				{
					Settings.setReadyDiodeGreenBlink(false);               //  Запретить мигание светодиода "Готов" (зеленый)   
					Settings.setReadyDiodeRedBlink(false);                 //  Запретить мигание светодиода "Готов" (красный)  
					Settings.setReadyDiodeOrangeBlink(true);               //  Разрешить мигание светодиода "Готов" (оранжевый)   
				}
				else if (val_led >= 50)                                    // определен максимальный уровень заряда аккумулятора
				{
					Settings.setReadyDiodeOrangeBlink(false);              //  Запретить мигание светодиода "Готов" (оранжевый)   
					Settings.setReadyDiodeGreenBlink(true);                //  Разрешить мигание светодиода "Готов" (зеленый)   
					Settings.setReadyDiodeRedBlink(false);                 //  Запретить мигание светодиода "Готов" (красный)  
				}
			}
		}
		else
		{  // Отобразить состояние заряда аккумулятора постоянным свечением после установления связи с Iridium
			Settings.setReadyDiodeGreenBlink(false);                       //  Запретить мигание светодиода "Готов" (зеленый)   
			Settings.setReadyDiodeRedBlink(false);                         //  Запретить мигание светодиода "Готов" (красный)  

			if (val_led < 15)
			{
				digitalWrite(LED_RED_READY, HIGH);
				digitalWrite(LED_GREEN_READY, LOW);
			}
			else if ((val_led >= 15) && (val_led < 50))
			{
				digitalWrite(LED_RED_READY, HIGH);
				digitalWrite(LED_GREEN_READY, HIGH);
			}
			else if (val_led >= 50)
			{
				digitalWrite(LED_RED_READY, LOW);
				digitalWrite(LED_GREEN_READY, HIGH);
			}
		}
	}
	else
	{
		digitalWrite(LED_RED_READY, LOW);
		digitalWrite(LED_GREEN_READY, LOW);
		analogWrite(KEY_LED_SOS, 0);                        // Ключ управления подсветкой кнопок ШИМ сигналом
		analogWrite(KEY_LED_POINT, 0);                      // Ключ включения подсветки кнопок 
		Settings.displayBacklight(false);                   // выключаем подсветку  дисплея
	}
}




//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDMenuScreen
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDMenuScreen::LCDMenuScreen()
 {
	
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDMenuScreen::~LCDMenuScreen()
 {
	 pressed_button = -1;

 }
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenuScreen::onActivate(LCDMenu* menuManager)
{
	pressed_button = -1;
	count_pressed_button = 0;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenuScreen::setup(LCDMenu* menuManager)
{
	 Settings.SetButtonRetention(false);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenuScreen::onButtonPressed(LCDMenu* menuManager, int buttonID)
{

	pressed_button = buttonID;
	//DBGLN("LCDMainScreen::onButtonPressed..");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenuScreen::onButtonReleased(LCDMenu* menuManager, int buttonID)
{
	released_button = buttonID;
	//DBGLN("LCDMenuScreen::onButtonReleased..");
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDMenuScreen::onButtonisRetention(LCDMenu* menuManager, int buttonID)
{
	//DBG("LCDMainScreen::onButtonisRetention..");
}


void LCDMenuScreen::update(LCDMenu* menuManager)
 {
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера
    #endif 

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

   LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран


  if(pressed_button != -1)
  {
    if(pressed_button == BUTTON_LEFT)
    {
		pressed_button = -1;
		if (count_pressed_button > 1 )
		{
			count_pressed_button--;
			char msgNum[4] = "";

			itoa(count_pressed_button, msgNum, 10);                     // конвертируем из числа с указанием базиса (десятичный)
			dc->setCursor(0, 1);                                           // Установить курсор в позицию на зкране(0, 1);
			dc->print("Menu ");
			dc->print(msgNum);
			dc->print("               ");
			dc->setCursor(0, 2);                                           // Установить курсор в позицию на зкране(0, 2);
			dc->print("                    ");
			dc->setCursor(0, 3);                                           // Установить курсор в позицию на зкране(0, 3);
			dc->print("                    ");
		}
    }
    else
    if(pressed_button == BUTTON_RIGHT)
    {
		pressed_button = -1;
		count_pressed_button++;
		if (count_pressed_button > 9)
		{
			count_pressed_button = 9;
		}

		char msgNum[3] = "";
		itoa(count_pressed_button, msgNum, 10);                        // конвертируем из числа с указанием базиса (десятичный)
		dc->setCursor(0, 1);                                           // Установить курсор в позицию на зкране(0, 0);
		dc->print("Menu ");
		dc->print(msgNum);
		dc->print("               ");
		dc->setCursor(0, 2);                                           // Установить курсор в позицию на зкране(0, 1);
		dc->print("                    ");
		dc->setCursor(0, 3);                                           // Установить курсор в позицию на зкране(0, 1);
		dc->print("                    ");
    }
    else
    if(pressed_button == BUTTON_POINT)
    {
		pressed_button = -1;
		switch (count_pressed_button)
		{
		case 0:
			//выполняется, когда count_pressed_button равно 0
		//	DBGLN("RETURN MAIN");
			menuManager->switchToScreen("MAIN");
			break;
		case 1:
			//выполняется, когда count_pressed_button равно 1
			//DBGLN("CLEAR MESSAGE");
			menuManager->switchToScreen("ClearMessage");
			break;
		case 2:
			menuManager->switchToScreen("ViewVersion");
			//DBGLN("PROGRAMM VERSION");
			//выполняется когда  count_pressed_button равно 2
			break;
		case 3:
			//DBGLN("SET TIME DISPLAY OFF");
			menuManager->switchToScreen("SetTimeLedLCDOff");
			//выполняется, когда count_pressed_button равно 3
			break;
		case 4:
			//DBGLN("SET TIME MAIN MENU");
			menuManager->switchToScreen("SetTimeMainMenu");
			//выполняется когда  count_pressed_button равно 4
			break;
		case 5:
		//	DBGLN("SET TIME POWER AKK");
			menuManager->switchToScreen("SetTimeAkkOff");
			//выполняется, когда count_pressed_button равно 5
			break;
		case 6:
			//DBGLN("SET LED BRIGHTNESS");
			menuManager->switchToScreen("SetLEDbrightness");
			//выполняется когда  count_pressed_button равно 6
			break;
		case 7:
			//DBGLN("SET OPT BRIGHTNESS");
			menuManager->switchToScreen("SetOPTbrightness");
			//выполняется, когда count_pressed_button равно 7
			break;
		case 8:
			//DBGLN(Confirmations sent); 
			menuManager->switchToScreen("ConfirmationsSent");
			//выполняется когда  count_pressed_button равно 8
			break;
		case 9:
			//DBGLN(Factory_settings); 
			menuManager->switchToScreen("FactorySettings");
			//выполняется, когда count_pressed_button равно 9
			break;
		case 10:
			//DBGLN(Factory_settings); 
			//menuManager->switchToScreen("FactorySettings");
			//выполняется когда  count_pressed_button равно 10
			break;
		//case 11:

		//	DBGLN(count_pressed_button);
		//	//выполняется, когда count_pressed_button равно 11
		//	break;
		//case 12:

		//	DBGLN(count_pressed_button);
		//	//выполняется когда  count_pressed_button равно 12
		//	break;
		//case 13:

		//	DBGLN(count_pressed_button);
		//	//выполняется, когда count_pressed_button равно 13
		//	break;
		//case 14:

		//	DBGLN(count_pressed_button);
		//	//выполняется когда  count_pressed_button равно 14
		//	break;
		//case 15:

		//	DBGLN(count_pressed_button);
		//	//выполняется, когда count_pressed_button равно 15
		//	break;
			default:
			break;
			// выполняется, если не выбрана ни одна альтернатива
			// default необязателен
		}
     // menuManager->switchToScreen("MEMINIT");
    }

	// Отобразить пункты меню
	switch (count_pressed_button) 
	{

	case 1:
		//выполняется, когда count_pressed_button равно 1
		dc->printRus("УДАЛИТЬ СООБЩЕНИЯ", 2, 2);
		break;
	case 2:
		dc->printRus("ВЕРСИЯ ПРОГРАММЫ", 2, 2);
		//выполняется когда  count_pressed_button равно 2
		break;
	case 3:
		dc->printRus(" УСТАНОВИТЬ ВРЕМЯ", 0, 2);
		dc->printRus("ОТКЛЮЧЕНИЯ ДИСПЛЕЯ", 0, 3);
		//выполняется, когда count_pressed_button равно 3
		break;
	case 4:
		dc->printRus("  УСТАНОВИТЬ ВРЕМЯ", 0, 1);
		dc->printRus(" ВОЗВРАТА В ", 0, 2);
		dc->printRus(" ОСНОВНУЮ ПРОГРАММУ", 0, 3);
		//выполняется когда  count_pressed_button равно 4
		break;
	case 5:
		dc->printRus(" УСТАНОВИТЬ ВРЕМЯ", 0, 2);
		dc->printRus("РАБОТЫ АККУМУЛЯТОРА", 0, 3);
		//dc->setCursor(0, 2);                                           // Установить курсор в позицию на зкране(0, 2);
		//dc->print("SET TIME POWER AKK");
		//DBGLN(count_pressed_button);
		//выполняется, когда count_pressed_button равно 5
		break;
	case 6:
		dc->printRus("УСТАНОВИТЬ ЯРКОСТЬ", 1, 2);
		dc->printRus("ПОДСВЕТКИ КНОПОК", 2, 3);
		//выполняется когда  count_pressed_button равно 6
		break;
	case 7:
		dc->printRus("УСТАНОВИТЬ ПОРОГ", 1, 2); 
		dc->printRus("ДАТЧИКА ЯРКОСТИ", 1, 3);
		//выполняется, когда count_pressed_button равно 7
		break;
	case 8:

		dc->printRus(" ПОДТВЕРЖДЕНИЯ,", 0, 1);
		dc->printRus("ОТПРАВЛЕННЫЕ В", 1, 2);
		dc->printRus("БУФЕР ТРЕКЕРА", 1, 3);
		//выполняется когда  count_pressed_button равно 8
		break;
	case 9:
		dc->printRus("ЗАВОДСКИЕ УСТАНОВКИ", 0, 2);
		//выполняется, когда count_pressed_button равно 9
		break;
	case 10:

		//выполняется когда  count_pressed_button равно 10
		break;
	//case 11:

	//	DBGLN(count_pressed_button);
	//	//выполняется, когда count_pressed_button равно 11
	//	break;
	//case 12:

	//	DBGLN(count_pressed_button);
	//	//выполняется когда  count_pressed_button равно 12
	//	break;
	//case 13:

	//	DBGLN(count_pressed_button);
	//	//выполняется, когда count_pressed_button равно 13
	//	break;
	//case 14:

	//	DBGLN(count_pressed_button);
	//	//выполняется когда  count_pressed_button равно 14
	//	break;
	//case 15:

	//	DBGLN(count_pressed_button);
	//	//выполняется, когда count_pressed_button равно 15
	//	break;
	default:
		break;
		// выполняется, если не выбрана ни одна альтернатива
		// default необязателен
	}
  }
}
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDMenuScreen::draw(LCDMenu* menuManager)
 {
	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	// MenuRus
	 dc->clear();                             // Стереть экран для вывода новой информации
	 dc->printRus("СЕРВИСНОЕ МЕНЮ", 3, 0);

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeLedLCDOff
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetTimeLedLCDOff* TimeLCDTick = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


 LCDSetTimeLedLCDOff::LCDSetTimeLedLCDOff()
 {
	 tickerButton = -1;
	 TimeLCDTick = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetTimeLedLCDOff::~LCDSetTimeLedLCDOff()
 {
   
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::onButtonPressed(LCDMenu* menuManager, int buttonID)
 {
	 pressed_button = buttonID;
	 tickerButton = -1;
	 if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	 {
		 tickerButton = buttonID;
		 Ticker.start(this);
	 }
 }
 
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::onButtonReleased(LCDMenu* menuManager, int buttonID)
 {
	 released_button = buttonID;
	 Ticker.stop();
	 tickerButton = -1;
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::onActivate(LCDMenu* menuManager)  // Отобразить текущие данные при запуске функции
 {
	 timeLCD = Settings.GetTimeLedLCD();
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::onTick()
 {
	 if (tickerButton == BUTTON_LEFT)
		 incTimeLCD(-1);
	 else
		 if (tickerButton == BUTTON_RIGHT)
			 incTimeLCD(1);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::incTimeLCD(int val)
 {
	 //LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	 timeLCD = Settings.GetTimeLedLCD();
	 timeLCD += val;

	 if (timeLCD < 1)
	 {
		 timeLCD = 1;
	 }
	 if (timeLCD > MAX_BACKLIGHT_OFF_DELAY)
	 {
		 timeLCD = MAX_BACKLIGHT_OFF_DELAY;
	 }
	 Settings.SetTimeLedLCD(timeLCD);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::setup(LCDMenu* menuManager)
 {
 
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::update(LCDMenu* menuManager)
 {
	 #ifdef USE_WATCHDOG_TIMER 
	 Settings.reset_IWDG();      // Сброс сторожевого таймера
     #endif 

	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран

	 timeLCD = Settings.GetTimeLedLCD();
	 if (timeLCD != timeLCD_tmp)
	 {
		 char msgNum[3] = "";
		 itoa(timeLCD, msgNum, 10);                    // конвертируем из числа с указанием базиса (десятичный)
		 dc->printRus(" <  СЕКУНД ", 1, 3);
		 dc->print(msgNum);
		 dc->print("  >");
	 }

	 if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	 {
		 Ticker.tick();
	 }

	 if (pressed_button != -1)
	 {
		timeLCD = Settings.GetTimeLedLCD();
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			incTimeLCD(-1);
		}
		else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			incTimeLCD(1);
		}

	 }
  
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeLedLCDOff::draw(LCDMenu* menuManager)
 {
	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }


	 dc->setCursor(0, 3);
	 dc->print("                    ");
	 char msgNum[3] = "";
	 itoa(timeLCD, msgNum, 10);          // конвертируем из числа с указанием базиса (десятичный)
	 dc->printRus(" <  СЕКУНД ", 1, 3);
	 dc->print(msgNum);
	 dc->print("  >");

	// DBGLN("LCDSetTimeLedLCDOff");		 // Проверить записанное сообщение. 
 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeAkkOff
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDSetTimeAkkOff* TimeAkkTick = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 LCDSetTimeAkkOff::LCDSetTimeAkkOff()
 {
	 tickerButton = -1;
	 TimeAkkTick = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetTimeAkkOff::~LCDSetTimeAkkOff()
 {
  
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::onActivate(LCDMenu* menuManager)
 {
	 timeAkk = Settings.GetTimeAkk();
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::onButtonPressed(LCDMenu* menuManager, int buttonID)
 {
	 pressed_button = buttonID;
	 tickerButton = -1;
	 if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	 {
		 tickerButton = buttonID;
		 Ticker.start(this);
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 void LCDSetTimeAkkOff::onButtonReleased(LCDMenu* menuManager, int buttonID)
 {
	 released_button = buttonID;
	 Ticker.stop();
	 tickerButton = -1;
	 DBGLN("LCDSetTimeAkkOff::onButtonReleased..");
 }
 
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::onTick()
 {
	 if (tickerButton == BUTTON_LEFT)
		 incAkk(-1);
	 else
		 if (tickerButton == BUTTON_RIGHT)
			 incAkk(1);
	 // DBG("onTick.. ");		 // Проверить записанное сообщение. 
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::incAkk(int val)
 {
	 //LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	 timeAkk = Settings.GetTimeAkk();

	 timeAkk += val;

	 if (timeAkk < 1)
	 {
		 timeAkk = 1;
	 }
	 if (timeAkk > MAX_POWER_TIME)
	 {
		 timeAkk = MAX_POWER_TIME;
	 }
	 Settings.SetTimeAkk(timeAkk);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::setup(LCDMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::update(LCDMenu* menuManager)
 {
	 #ifdef USE_WATCHDOG_TIMER 
	 Settings.reset_IWDG();      // Сброс сторожевого таймера
     #endif

	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран

	 timeAkk = Settings.GetTimeAkk();
	 if (timeAkk != timeAkk_tmp)
	 {
		 timeAkk_tmp = timeAkk;
		 char msgNum[3] = "";
		 itoa(timeAkk, msgNum, 10);                                       // конвертируем из числа с указанием базиса (десятичный)
		// dc->setCursor(0, 3);                                             // Установить курсор в позицию на зкране
		 dc->printRus("< ВРЕМЯ ЧАС - ", 0, 3);
		 //dc->print("< TIME HOUR - ");
		 dc->print(msgNum);
		 dc->print(" > ");
	 }

	 if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	 {
		 Ticker.tick();
	 }

    if (pressed_button != -1)
    {
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			incAkk(-1);
		}
		else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1; 
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			incAkk(1);
		}

    }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeAkkOff::draw(LCDMenu* menuManager)
 {
	LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 char msgNum[3] = "";
	 itoa(timeAkk, msgNum, 10);                   // конвертируем из числа с указанием базиса (десятичный)
	 dc->printRus("< ВРЕМЯ ЧАС - ", 0, 3);
	 dc->print(msgNum);
	 dc->print(" > ");

	// DBGLN("LCDSetTimeAkkOff");		 // Проверить записанное сообщение. 
	 delay(1000);

 }

//***********************************************
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetLEDbrightness
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetLEDbrightness* LEDFactor = NULL;
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 LCDSetLEDbrightness::LCDSetLEDbrightness()
 {
	 tickerButton = -1;
	 LEDFactor = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetLEDbrightness::~LCDSetLEDbrightness()
 {
	 Settings.setLEDmenu(false);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::onActivate(LCDMenu* menuManager)
 {
	 Settings.setLEDmenu(true);
	 LED_Brightness = Settings.getLED_Brightness();
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::onButtonPressed(LCDMenu* menuManager, int buttonID)
 {
	 pressed_button = buttonID;
	 tickerButton = -1;
	 if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	 {
		 tickerButton = buttonID;
		 Ticker.start(this);
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 void LCDSetLEDbrightness::onButtonReleased(LCDMenu* menuManager, int buttonID)
 {
	 released_button = buttonID;
	 Ticker.stop();
	 tickerButton = -1;
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::onTick()
 {
	 if (tickerButton == BUTTON_LEFT)
		 incFactorLED(-1);
	 else
		 if (tickerButton == BUTTON_RIGHT)
			 incFactorLED(1);
	// DBGLN("onTick..");
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::incFactorLED(int val)
 {
	 //LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	int LED_BrightnessOn = Settings.getLED_Brightness();

	LED_BrightnessOn += val;
	 if (LED_BrightnessOn < 2)
	 {
		 LED_BrightnessOn = 2;
	 }

	 if (LED_BrightnessOn > 255) //!!
	 {
		 LED_BrightnessOn = 255;
	 }
	 //DBG("onTick..");
	 //DBGLN(LED_BrightnessOn);
	 analogWrite(KEY_LED_SOS, LED_BrightnessOn);                // Ключ управления подсветкой кнопок ШИМ сигналом
	 analogWrite(KEY_LED_POINT, LED_BrightnessOn);              // Ключ включения подсветки кнопок 
	 Settings.setLED_Brightness(LED_BrightnessOn);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::setup(LCDMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::update(LCDMenu* menuManager)
 {
	 #ifdef USE_WATCHDOG_TIMER 
	 Settings.reset_IWDG();      // Сброс сторожевого таймера 
     #endif

	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	 LED_Brightness = Settings.getLED_Brightness();

	 if (LED_Brightness != LED_Brightness_tmp)
	 {
		 LED_Brightness_tmp = LED_Brightness;
		 itoa(LED_Brightness, msgNum, 10);
		 dc->setCursor(8, 3);
		 dc->print("  ");
		 if (LED_Brightness < 10)
			 dc->setCursor(10, 3);
		 else if ((LED_Brightness > 9) && (LED_Brightness < 100))
			 dc->setCursor(9, 3);
		 else if (LED_Brightness > 99)
			 dc->setCursor(8, 3);
		 dc->print(msgNum);
	 }

	 if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	 {
		 Ticker.tick();
	 }

	 if (pressed_button != -1)
	 {
		 if (pressed_button == BUTTON_LEFT)
		 {
			 pressed_button = -1;

			 incFactorLED(-1);
		 }
		 else
		 if (pressed_button == BUTTON_POINT)
		 {
			pressed_button = -1;
			Settings.setLEDmenu(false);             // Флаг разрешения управления подсветкой кнопок
			menuManager->switchToScreen("MENU");
		 }
		 else
		 if (pressed_button == BUTTON_RIGHT)
		 {
			pressed_button = -1;
			incFactorLED(1);
		 }
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetLEDbrightness::draw(LCDMenu* menuManager)
 {
	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }
	// LED_Brightness = Settings.getLED_Brightness();
	 dc->setCursor(0, 2);
	 dc->print("                    ");
	 dc->setCursor(0, 3);
	 dc->print("                    ");
	// dc->setCursor(1, 2);
	 dc->printRus("УСТАНОВИТЬ ЯРКОСТЬ", 1, 2);
	// dc->print("SET LED brightness");
	 itoa(LED_Brightness, msgNum, 10);
 	 dc->setCursor(1, 3);                                           // Установить курсор в позицию на зкране(0, 0);
	 dc->print("<  ");
	 dc->setCursor(8, 3);
	 dc->print("   ");
	 dc->setCursor(8, 3);
	 if (LED_Brightness < 10)
	 dc->setCursor(10, 3);
	 else if((LED_Brightness > 9)&&(LED_Brightness<100))
		 dc->setCursor(9, 3);
	 else if(LED_Brightness > 99)
		 dc->setCursor(8, 3);
	 dc->print(msgNum);
	 dc->setCursor(17, 3);
	 dc->print(" > ");
	 analogWrite(KEY_LED_SOS, LED_Brightness);                // Ключ управления подсветкой кнопок ШИМ сигналом
	 analogWrite(KEY_LED_POINT, LED_Brightness);              // Ключ включения подсветки кнопок 
 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetOPTbrightness
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetOPTbrightness* LuxFactor = NULL;
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 LCDSetOPTbrightness::LCDSetOPTbrightness()
 {
	 tickerButton = -1;
	 LuxFactor = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetOPTbrightness::~LCDSetOPTbrightness()
 {
	// Settings.setLEDmenu(false);
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::onActivate(LCDMenu* menuManager)
 {
	
	 OPT_Brightness = Settings.getLuxOptModule();
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::onButtonPressed(LCDMenu* menuManager, int buttonID)
 {
	 pressed_button = buttonID;
	 tickerButton = -1;
	// DBGLN("onButtonPressed");
	 if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	 {
		 tickerButton = buttonID;
		 Ticker.start(this);
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 void LCDSetOPTbrightness::onButtonReleased(LCDMenu* menuManager, int buttonID)
 {

	 released_button = buttonID;
	 Ticker.stop();
	 tickerButton = -1;
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::onTick()
 {
	 if (tickerButton == BUTTON_LEFT)
	 {
		 inc_OPT_LUX(-10);
	 }
	
	else
	if (tickerButton == BUTTON_RIGHT)
	{
		inc_OPT_LUX(10);
	}
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::inc_OPT_LUX(int val)
 {
	// DBG("incFactorOPT..");
	 uint16_t OPT_BrightnessInc = Settings.getLuxOptModule();
/*	 DBG(OPT_BrightnessInc);
	 DBG("/")*/;
	 //LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	 OPT_BrightnessInc += val;
	 if (OPT_BrightnessInc < MIN_LUX_OPT_MODULE)
	 {
		 OPT_BrightnessInc = MIN_LUX_OPT_MODULE;
	 }

	 if (OPT_BrightnessInc > MAX_LUX_OPT_MODULE)
	 {
		 OPT_BrightnessInc = MAX_LUX_OPT_MODULE;
	 }
	// DBGLN(OPT_BrightnessInc);
	 Settings.setLuxOptModule(OPT_BrightnessInc);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::setup(LCDMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::update(LCDMenu* menuManager)
 {
	 #ifdef USE_WATCHDOG_TIMER 
	 Settings.reset_IWDG();      // Сброс сторожевого таймера
     #endif

	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	 OPT_Brightness = Settings.getLuxOptModule();

	 if (OPT_Brightness != OPT_Brightness_tmp)
	 {
		 OPT_Brightness_tmp = OPT_Brightness;
		 itoa(OPT_Brightness, msgNum, 10);
		 dc->setCursor(6, 3);
		 dc->print("      ");
		 if (OPT_Brightness < 10)
			 dc->setCursor(10, 3);
		 else if ((OPT_Brightness > 9) && (OPT_Brightness < 100))
			 dc->setCursor(9, 3);
		 else if ((OPT_Brightness > 99) && (OPT_Brightness < 999))
			 dc->setCursor(8, 3);
		 else if ((OPT_Brightness > 999) && (OPT_Brightness < 9999))
			 dc->setCursor(7, 3);
		 else if (OPT_Brightness > 9999)
			 dc->setCursor(6, 3);

		 dc->print(msgNum);
	 }

	 if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	 {
		 Ticker.tick();
	 }

	 if (pressed_button != -1)
	 {
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			inc_OPT_LUX(-1);
		}
		else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			Settings.setLEDmenu(false);             // Флаг разрешения управления подсветкой кнопок
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			inc_OPT_LUX(1);
		}
	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetOPTbrightness::draw(LCDMenu* menuManager)
 {
	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 int OPT_Brightness = Settings.getLuxOptModule();
	 dc->setCursor(0, 2);
	 dc->print("                    ");
	 dc->setCursor(0, 3);
	 dc->print("                    ");
	// dc->setCursor(1, 2);
	 dc->printRus("УСТАНОВИТЬ ПОРОГ", 2, 2);
	 itoa(OPT_Brightness, msgNum, 10);
	 dc->setCursor(1, 3);                                           // Установить курсор в позицию на зкране(0, 0);
	 dc->print("<  ");
	 itoa(OPT_Brightness, msgNum, 10);
	 dc->setCursor(6, 3);
	 dc->print("      ");
	 if (OPT_Brightness < 10)
		 dc->setCursor(10, 3);
	 else if ((OPT_Brightness > 9) && (OPT_Brightness < 100))
		 dc->setCursor(9, 3);
	 else if ((OPT_Brightness > 99) && (OPT_Brightness < 999))
		 dc->setCursor(8, 3);
	 else if ((OPT_Brightness > 999) && (OPT_Brightness < 9999))
		 dc->setCursor(7, 3);
	 else if (OPT_Brightness > 9999)
		 dc->setCursor(6, 3);
	 dc->print(msgNum);
	 dc->setCursor(17, 3);
	 dc->print(" > ");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDSetTimeMainMenu
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetTimeMainMenu* TimeMainMenuTick = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

 LCDSetTimeMainMenu::LCDSetTimeMainMenu()
 {
	 tickerButton = -1;
	 TimeMainMenuTick = this;
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 LCDSetTimeMainMenu::~LCDSetTimeMainMenu()
 {
	
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::onActivate(LCDMenu* menuManager)
 {
	 timeMainMenu = Settings.GetTimeReturnMainMenu();
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::onButtonPressed(LCDMenu* menuManager, int buttonID)
 {
	 pressed_button = buttonID;
	 tickerButton = -1;
	 if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	 {
		 tickerButton = buttonID;
		 Ticker.start(this);
	 }
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::onButtonReleased(LCDMenu* menuManager, int buttonID)
 {
	 released_button = buttonID;
	 Ticker.stop();
	 tickerButton = -1;
	// DBGLN("LCDSetTimeMainMenu::onButtonReleased..");
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::onTick()
 {
	if (tickerButton == BUTTON_LEFT)
		incTimeMain(-1);
	else
	if (tickerButton == BUTTON_RIGHT)
		incTimeMain(1);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::incTimeMain(int val)
 {
	 timeMainMenu = Settings.GetTimeReturnMainMenu();
	 timeMainMenu += val;

	 if (timeMainMenu < 1)
	 {
		 timeMainMenu = 1;
	 }
	 if (timeMainMenu > MAX_TO_MAIN_SCREEN_DELAY)
	 {
		 timeMainMenu = MAX_TO_MAIN_SCREEN_DELAY;
	 }
	 Settings.SetTimeReturnMainMenu(timeMainMenu);
 }

 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::setup(LCDMenu* menuManager)
 {

 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::update(LCDMenu* menuManager)
 {
	 #ifdef USE_WATCHDOG_TIMER 
	 Settings.reset_IWDG();      // Сброс сторожевого таймера
     #endif

	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	 LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран

	 timeMainMenu = Settings.GetTimeReturnMainMenu();
	 if (timeMainMenu != timeMainMenu_tmp)
	 {
		timeMainMenu_tmp = timeMainMenu;

		char msgNum[3] = "";
		itoa(timeMainMenu, msgNum, 10);
		dc->printRus(" <  СЕКУНД ", 1, 3);
		dc->print(msgNum);
		dc->print("  >");
	 }

	 if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	 {
		 Ticker.tick();
	 }

	 if (pressed_button != -1)
	 {
		 timeMainMenu = Settings.GetTimeReturnMainMenu();
		 if (pressed_button == BUTTON_LEFT)
		 {
			 pressed_button = -1;
			 incTimeMain(-1);
		 }
		 else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			incTimeMain(1);
		}

	 }
 }
 //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 void LCDSetTimeMainMenu::draw(LCDMenu* menuManager)
 {
	 LCD_Class* dc = menuManager->getDC();

	 if (!dc)
	 {
		 return;
	 }

	/* dc->setCursor(0, 1);
	 dc->print("                    ");*/
	 dc->setCursor(0, 2);
	 dc->print("                    ");
	 dc->printRus("ВОЗВРАТА", 2, 2);
	 dc->setCursor(0, 3);
	 dc->print("                    ");
	 char msgNum[4] = "";
	 itoa(timeMainMenu, msgNum, 10);
	 dc->printRus(" <  СЕКУНД ", 1, 3);
	 dc->print(msgNum);
	 dc->print("  >");

	// DBGLN("LCDSetTimeMainMenu");		 // Проверить записанное сообщение. 
	 delay(1000);

 }

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDClearMessageMenu
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDClearMessageMenu::LCDClearMessageMenu()
{
	//tickerButton = -1;
	//pressed_button = -1;
	//pressed_button_Retention = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDClearMessageMenu::~LCDClearMessageMenu()
{
	//pressed_button = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDClearMessageMenu::onActivate(LCDMenu* menuManager)
{
	pressed_button = -1;
	count_pressed_button = 1;
	//count_pressed_button_tmp = 1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDClearMessageMenu::setup(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	//Settings.SetButtonRetention(false);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDClearMessageMenu::onButtonPressed(LCDMenu* menuManager, int buttonID)
{

	pressed_button = buttonID;
	//DBGLN("LCDMainScreen::onButtonPressed..");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDClearMessageMenu::onButtonReleased(LCDMenu* menuManager, int buttonID)
{

	/*released_button = buttonID;
	DBGLN("LCDClearMessageMenu::onButtonReleased..");*/
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//void LCDClearMessageMenu::onButtonisRetention(LCDMenu* menuManager, int buttonID)
//{
//	DBG("LCDClearMessageMenu::onButtonisRetention..");
//
//}


void LCDClearMessageMenu::update(LCDMenu* menuManager)
{
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера
    #endif

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	if (pressed_button != -1)
	{
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			if (count_pressed_button > 1)
			{
				count_pressed_button--;
			}
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			count_pressed_button++;
			if (count_pressed_button > 2)
			{
				count_pressed_button = 2;
			}
			DBG("BUTTON_RIGHT - ");
			DBGLN(count_pressed_button);
		}
		else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			switch (count_pressed_button)
			{
			case 1:
				//выполняется, когда count_pressed_button равно 1
				DBGLN("RETURN MAIN");
				dc->powerMode(SSD1311_LCD_OFF);
				dc->cursor_on = false;                      // 
				dc->cursor_blinking = false;                      // 
				dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
				dc->BDC = false;
				dc->setEntryMode();
				dc->powerMode(SSD1311_LCD_ON);
				menuManager->switchToScreen("MENU");
				//выполняется когда  count_pressed_button равно 2
				break;
			case 2:
				ClearMessageM(menuManager);
				DBGLN(F("EEPROM clearance END"));
				menuManager->switchToScreen("MENU");
				//выполняется когда  count_pressed_button равно 2
				break;
			default:
				break;
				// выполняется, если не выбрана ни одна альтернатива
				// default необязателен
			}
		}

		// Отобразить пункты меню
		switch (count_pressed_button)
		{

		case 1:
			//выполняется, когда count_pressed_button равно 1
			 dc->setCursor(3, 2);                                           // Установить курсор в позицию на зкране(3, 2);
			break;
		case 2:
			dc->setCursor(10, 2);                                           // Установить курсор в позицию на зкране(10, 2);
			//выполняется когда  count_pressed_button равно 2
			break;

		default:
			break;
			// выполняется, если не выбрана ни одна альтернатива
			// default необязателен
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDClearMessageMenu::draw(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	dc->clear();                             // Стереть экран для вывода новой информации
	dc->printRus("УДАЛИТЬ СООБЩЕНИЯ", 1, 0);
	dc->printRus("ВЫХОД  УДАЛИТЬ", 3, 2);

	dc->powerMode(SSD1311_LCD_OFF);
	dc->cursor_on = false;                       // 
	dc->cursor_blinking = true;                      // 
	dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
	dc->BDC = false;
	dc->setEntryMode();
	dc->powerMode(SSD1311_LCD_ON);
	dc->setCursor(3, 2);                    // Установить курсор впозицию на зкране
	DBGLN("LCDClearMessageMenu");	        // Проверить записанное сообщение. 

}

void LCDClearMessageMenu::ClearMessageM(LCDMenu* menuManager)
{
	LCD_Class* dc = LCDScreen->getDC();

	if (!dc)
	{
		return;
	}

	uint32_t len = 0;

	// Определяем объем установленной памяти
   // LCDScreen->resetIdleTimer();
	len = Settings.getCurrentCountMessage();                     // Получить количество записанных сообщений
	len = (len * Number_of_bytes_block) + Response_message_block_ADDRESS;    // Расчитать конечный адрес сообщений 

	Settings.displayBacklight(true);                             // Включить подсветку дисплея

	int step_clear = len / 20;                                   // Шаг перемещения индикации стирания на дисплее
	int step_reset = 2048;                                       // Интералы сброса таймера ничего не деланья
	byte step_cursor = 0;                                        // 

	char str[20];
	itoa(len, str, 10);                                          // Записать в строку номер сообщения в памяти
	dc->clear();                                                 // Стереть экран
	dc->setCursor(0, 0);                                         // Установить курсор в начало экрана
	dc->printRus("СООБЩЕНИЯ УДАЛЯЮТСЯ", 0, 0);
	//dc->print("Start MESSAGE clear..");
	dc->setCursor(0, 1);
	dc->printRus("РАЗМЕР", 0, 1);
	//dc->print("Size - ");
	dc->setCursor(7, 1);
	dc->print(str);

	/* включить мигание курсора */
	dc->powerMode(SSD1311_LCD_OFF);                            // отключить дисплей на время настройки мигания знакоместа
	dc->cursor_on = false;                                     // подчеркивание курсора отключить
	dc->cursor_blinking = true;                                // оставить только мигание знакоместа
	dc->cursor_direction = SSD1311_DIRECTION_RIGHT;            // перемещение курсора вправо
	dc->BDC = false;                                           // не знаю, но применяю :-)
	dc->setEntryMode();                                        // ввести настройки в дисплей
	dc->powerMode(SSD1311_LCD_ON);                             // включить дисплей


	for (uint32_t address = Response_message_block_ADDRESS; address < len; address++)
	{
		MemWrite(address, 0x00);
#ifdef USE_WATCHDOG_TIMER
		Settings.reset_IWDG();      // Сброс сторожевого таймера 
#endif
		if ((address % step_clear) == 0)
		{
			itoa(address, str, 10);                              // Записать в строку номер текущего адреса стирания
			dc->setCursor(14, 1);                                // Установить курсор в начало экрана
			dc->print(str);                                      // отобразить номер текущего адреса стирания
			dc->setCursor(step_cursor, 2);                       // установить курсор
			step_cursor++;
			dc->setCursor(step_cursor - 1, 2);
			dc->print("X");
			dc->setCursor(step_cursor - 1, 2);
		}
		//step_reset таймер
		if ((address % step_reset) == 0)                         // периодически сбрасываем таймер ничего не деланья
		{
			LCDScreen->resetIdleTimer();
		}
	} // for

	delay(1000);

	uint32_t address = Start_confirmation_ADDRESS;
	byte count_confirmation = MemRead(Current_Counter_confirmation);   // Получить номер текущего подтверждения
	itoa(address, str, 10);                                          // Записать в строку номер сообщения в памяти
	dc->setCursor(0, 1);
	dc->printRus("РАЗМЕР", 0, 1);
	//dc->print("Size - ");
	dc->print(str);
	dc->setCursor(12, 1);                                // Установить курсор в начало экрана
	dc->print("        ");

	for (address; address < (Number_of_bytes_confirmation * count_confirmation) + Start_confirmation_ADDRESS; address++)
	{
		MemWrite(address, 0x00);
#ifdef USE_WATCHDOG_TIMER 
		Settings.reset_IWDG();      // Сброс сторожевого таймера 
#endif
		if ((address % step_clear) == 0)
		{
			itoa(address, str, 10);                              // Записать в строку номер сообщения в памяти
			dc->setCursor(14, 1);                                // Установить курсор в начало экрана
			dc->print(str);
			dc->setCursor(step_cursor, 2);
			step_cursor++;
			dc->setCursor(step_cursor - 1, 2);
			dc->print("X");
			dc->setCursor(step_cursor - 1, 2);
		}
	} // for

	delay(1000);
	//  DBGLN("End..");

	  //LCDScreen->resetIdleTimer();
	dc->powerMode(SSD1311_LCD_OFF);
	dc->cursor_on = false;                      // 
	dc->cursor_blinking = false;                      // 
	dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
	dc->BDC = false;
	dc->setEntryMode();
	dc->powerMode(SSD1311_LCD_ON);
	MemWrite(All_Count_Text_Message_ADDRESS, 0);                // адрес хранения счетчика общего количества записей
	MemWrite(Count_NotRead_Message_ADDRESS, 0);                 // адрес хранения счетчика не прочитанного количества записей
	MemWrite(Current_Counter_Message, 0);                       // адрес хранения текущего счетчика количества записей
	MemWrite(Flipping_Counter_Message, 0);                      // адрес хранения счетчика листания записей
  //  MemWrite(TmpCount_Flip_Message_ADDRESS, 0);               // адрес хранения временного счетчика количества записей
	MemWrite(Response_message_block_ADDRESS, 0);                // начальный адрес хранения счетчика ответных сообщений
	MemWrite(Current_Counter_confirmation, 0);                  // сохраним новый номер сохраненного подтверждения.

	dc->setCursor(0, 3);                                        // Установить курсор в начало экрана
    dc->printRus("ЗАВЕРШЕНО ", 0, 3);
	//dc->print("End..");                                         // Отобразить новое сообщение
	//DBGLN(F("EEPROM clearance END"));
   // LCDScreen->switchToScreen("MAIN");
   // CommandHandler.Stm32_SoftReset();
   // Stm32_SoftReset();
	delay(2000);
}



//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDFactorySettingMenu
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDFactorySettingMenu::LCDFactorySettingMenu()
{
	//tickerButton = -1;
	//pressed_button = -1;
	//pressed_button_Retention = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDFactorySettingMenu::~LCDFactorySettingMenu()
{
	//pressed_button = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDFactorySettingMenu::onActivate(LCDMenu* menuManager)
{
	pressed_button = -1;
	count_pressed_button = 1;
	//count_pressed_button_tmp = 1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDFactorySettingMenu::setup(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	//Settings.SetButtonRetention(false);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDFactorySettingMenu::onButtonPressed(LCDMenu* menuManager, int buttonID)
{

	pressed_button = buttonID;
	//DBGLN("LCDMainScreen::onButtonPressed..");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//void LCDFactorySettingMenu::onButtonReleased(LCDMenu* menuManager, int buttonID)
//{
//
//	//released_button = buttonID;
//	//DBGLN("LCDClearMessageMenu::onButtonReleased..");
//}

void LCDFactorySettingMenu::update(LCDMenu* menuManager)
{
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера 
    #endif

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	if (pressed_button != -1)
	{
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			if (count_pressed_button > 1)
			{
				count_pressed_button--;
			}
		}
		else
			if (pressed_button == BUTTON_RIGHT)
			{
				pressed_button = -1;
				count_pressed_button++;
				if (count_pressed_button > 2)
				{
					count_pressed_button = 2;
				}
				//DBG("BUTTON_RIGHT - ");
				//DBGLN(count_pressed_button);
			}
			else
				if (pressed_button == BUTTON_POINT)
				{
					pressed_button = -1;
					switch (count_pressed_button)
					{
					case 1:
						//выполняется, когда count_pressed_button равно 1
						//DBGLN("RETURN MAIN");
						dc->powerMode(SSD1311_LCD_OFF);
						dc->cursor_on = false;                      // 
						dc->cursor_blinking = false;                      // 
						dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
						dc->BDC = false;
						dc->setEntryMode();
						dc->powerMode(SSD1311_LCD_ON);
						menuManager->switchToScreen("MENU");
						//выполняется когда  count_pressed_button равно 2
						break;
					case 2:
						MemClear();
						//DBGLN(F("EEPROM clearance END"));
						menuManager->switchToScreen("MENU");
						//выполняется когда  count_pressed_button равно 2
						break;
					default:
						break;
						// выполняется, если не выбрана ни одна альтернатива
						// default необязателен
					}
				}

		// Отобразить пункты меню
		switch (count_pressed_button)
		{

		case 1:
			//выполняется, когда count_pressed_button равно 1
			dc->setCursor(5, 2);                                           // Установить курсор в позицию на зкране(3, 2);
			break;
		case 2:
			dc->setCursor(13, 2);                                           // Установить курсор в позицию на зкране(10, 2);
			//выполняется когда  count_pressed_button равно 2
			break;

		default:
			break;
			// выполняется, если не выбрана ни одна альтернатива
			// default необязателен
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDFactorySettingMenu::draw(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	dc->clear();                             // Стереть экран для вывода новой информации

	dc->printRus("УДАЛИТЬ СООБЩЕНИЯ", 1, 0);
	dc->printRus("ВЫХОД  УДАЛИТЬ", 3, 2);

	dc->powerMode(SSD1311_LCD_OFF);
	dc->cursor_on = false;                       // 
	dc->cursor_blinking = true;                      // 
	dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
	dc->BDC = false;
	dc->setEntryMode();
	dc->powerMode(SSD1311_LCD_ON);
	dc->setCursor(5, 2);                    // Установить курсор впозицию на зкране
	//DBGLN("LCDClearMessageMenu");	        // Проверить записанное сообщение. 

}



//**************************************************************************

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDViewVersion
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDViewVersion::LCDViewVersion()
{
	//tickerButton = -1;
	//pressed_button = -1;
	//pressed_button_Retention = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDViewVersion::~LCDViewVersion()
{
	//pressed_button = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDViewVersion::onActivate(LCDMenu* menuManager)
{
	pressed_button = -1;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDViewVersion::setup(LCDMenu* menuManager)
{

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDViewVersion::onButtonPressed(LCDMenu* menuManager, int buttonID)
{

	pressed_button = buttonID;

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//void LCDViewVersion::onButtonReleased(LCDMenu* menuManager, int buttonID)
//{
//
//	released_button = buttonID;
//	//DBGLN("LCDClearMessageMenu::onButtonReleased..");
//}

void LCDViewVersion::update(LCDMenu* menuManager)
{
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера 
    #endif

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	if (pressed_button != -1)
	{
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			menuManager->switchToScreen("MENU");
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDViewVersion::draw(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}
	dc->setCursor(0, 3);                                           // Установить курсор в позицию на зкране(0, 1);
	dc->print(SOFTWARE_VERSION);
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// LCDConfirmationsSent
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDConfirmationsSent* ConfirmationstTick = NULL;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

LCDConfirmationsSent::LCDConfirmationsSent()
{
	tickerButton = -1;
	ConfirmationstTick = this;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
LCDConfirmationsSent::~LCDConfirmationsSent()
{

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::onActivate(LCDMenu* menuManager)
{
	//timeAkk = Settings.GetTimeAkk();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::onButtonPressed(LCDMenu* menuManager, int buttonID)
{
	pressed_button = buttonID;
	tickerButton = -1;
	if (buttonID == BUTTON_LEFT || buttonID == BUTTON_RIGHT)
	{
		tickerButton = buttonID;
		Ticker.start(this);
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------

void LCDConfirmationsSent::onButtonReleased(LCDMenu* menuManager, int buttonID)
{
	released_button = buttonID;
	Ticker.stop();
	tickerButton = -1;
	//DBGLN("LCDSetTimeAkkOff::onButtonReleased..");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::onTick()
{
	if (tickerButton == BUTTON_LEFT)
		incConfirmationstAdrr(-1);
	else
		if (tickerButton == BUTTON_RIGHT)
			incConfirmationstAdrr(1);
	// DBG("onTick.. ");		 // Проверить записанное сообщение. 
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::incConfirmationstAdrr(int val)
{
	//LCDScreen->resetIdleTimer();                                       // Сбросить таймер перехода на главный экран
	//ConfirmationstCount = 0;/* Settings.GetTimeAkk();*/

	ConfirmationstCount += val;

	if (ConfirmationstCount < 1)
	{
		ConfirmationstCount = 0;
	}
	if (ConfirmationstCount > 90)
	{
		ConfirmationstCount = 90;
	}
	//Settings.SetTimeAkk(timeAkk);
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::setup(LCDMenu* menuManager)
{

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::update(LCDMenu* menuManager)
{
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера 
    #endif

	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	//timeAkk = Settings.GetTimeAkk();
	if (ConfirmationstCount != ConfirmationstCount_tmp)
	{
		ConfirmationstCount_tmp = ConfirmationstCount;
		char msgNum[3] = "";
		itoa(ConfirmationstCount, msgNum, 10);                           // конвертируем из числа с указанием базиса (десятичный)
		dc->printRus("<ПОДТВЕРЖДЕНИЯ ", 1, 1);
		dc->print(msgNum);
		if(ConfirmationstCount<10)
		   dc->print("> ");
		else
			dc->print(">");

		dc->setCursor(0, 2);                                                            // Установить курсор в начало экрана
		dc->print("                    ");
		dc->setCursor(0, 3);                                                            // Установить курсор в начало экрана
		dc->print("                    ");

		unsigned int cur_adr_conf = (Number_of_bytes_confirmation * ConfirmationstCount) + Start_confirmation_ADDRESS; //  получить  адрес текущего сообщения.
		MemReadChars(cur_adr_conf, msgConf, sizeof(msgConf));                 // Считать из памяти записанное сообщение.
		dc->setCursor(0, 2);                                                            // Установить курсор в начало экрана
		dc->print(msgConf);
	}

	if ((digitalRead(BUTTON_LEFT) == 0) || (digitalRead(BUTTON_RIGHT) == 0))// DBG("Butoon onTick.. ");
	{
		Ticker.tick();
	}

	if (pressed_button != -1)
	{
		if (pressed_button == BUTTON_LEFT)
		{
			pressed_button = -1;
			incConfirmationstAdrr(-1);
		}
		else
		if (pressed_button == BUTTON_POINT)
		{
			pressed_button = -1;
			menuManager->switchToScreen("MENU");
		}
		else
		if (pressed_button == BUTTON_RIGHT)
		{
			pressed_button = -1;
			incConfirmationstAdrr(1);
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void LCDConfirmationsSent::draw(LCDMenu* menuManager)
{
	LCD_Class* dc = menuManager->getDC();

	if (!dc)
	{
		return;
	}

	dc->setCursor(0, 1);
	dc->print("                    ");
	dc->setCursor(0, 2);
	dc->print("                    ");
	dc->setCursor(0, 3);
	dc->print("                    ");

	char msgNum[3] = "";
	itoa(ConfirmationstCount, msgNum, 10);                           // конвертируем из числа с указанием базиса (десятичный)
	dc->printRus("<ПОДТВЕРЖДЕНИЯ ", 1, 1);
	//dc->print("<Confirmations - ");
	dc->print(msgNum);
	if (ConfirmationstCount < 10)
		dc->print("> ");
	else
		dc->print(">");
	unsigned int cur_adr_conf = (Number_of_bytes_confirmation * ConfirmationstCount) + Start_confirmation_ADDRESS; //  получить  адрес текущего сообщения.
	MemReadChars(cur_adr_conf, msgConf, sizeof(msgConf));                 // Считать из памяти записанное сообщение.
	dc->setCursor(0, 2);
	dc->print("                    ");
	dc->setCursor(0, 2);                                                            // Установить курсор в начало экрана
	dc->print(msgConf);

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// TickerClass
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass Ticker;
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass::TickerClass()
{
  started = false;
  beforeStartTickInterval = 1000;
  tickInterval = 200;
  waitBefore = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
TickerClass::~TickerClass()
{
  stop();
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::setIntervals(uint16_t _beforeStartTickInterval,uint16_t _tickInterval)
{
  beforeStartTickInterval = _beforeStartTickInterval;
  tickInterval = _tickInterval;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::start(ITickHandler* h)
{
  if(started)
    return;

  handler = h;

  timer = millis();
  waitBefore = true;
  started = true;
 // DBG("TickerClass::start..");

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::stop()
{
  if(!started)
    return;

  handler = NULL;

  started = false;
  waitBefore = true;
 // DBGLN("TickerClass::stop..");
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TickerClass::tick()
{
  if(!started)
    return;

  uint32_t now = millis();

  if(waitBefore)
  {
    if(now - timer > beforeStartTickInterval)
    {
      waitBefore = false;
      timer = now;
      if(handler)
        handler->onTick();
	   // DBGLN("handler->onTick..");
    }
    return;
  }

  if(now - timer > tickInterval)
  {
    timer = now;
    if(handler)
      handler->onTick();
  }

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
