#include "Settings.h"
#include "LCDMenu.h"
#include "Memory.h"
#include "Configuration_STM32.h"  // Основные настройки программы
#include <malloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <Arduino.h>


INA219 ina;                       // присвоить переменную монитора аккумулятора




#ifdef USE_WATCHDOG_TIMER 
IWDG_HandleTypeDef hiwdg;
#endif 


//--------------------------------------------------------------------------------------------------------------------------------
// Монитор аккумулятора
//--------------------------------------------------------------------------------------------------------------------------------
void checkConfig()
{
	DBG("Mode:                 ");
	switch (ina.getMode())
	{
	case INA219_MODE_POWER_DOWN:      DBGLN("Power-Down"); break;
	case INA219_MODE_SHUNT_TRIG:      DBGLN("Shunt Voltage, Triggered"); break;
	case INA219_MODE_BUS_TRIG:        DBGLN("Bus Voltage, Triggered"); break;
	case INA219_MODE_SHUNT_BUS_TRIG:  DBGLN("Shunt and Bus, Triggered"); break;
	case INA219_MODE_ADC_OFF:         DBGLN("ADC Off"); break;
	case INA219_MODE_SHUNT_CONT:      DBGLN("Shunt Voltage, Continuous"); break;
	case INA219_MODE_BUS_CONT:        DBGLN("Bus Voltage, Continuous"); break;
	case INA219_MODE_SHUNT_BUS_CONT:  DBGLN("Shunt and Bus, Continuous"); break;
	default: DBGLN("unknown");
	}

	DBG("Range:                ");
	switch (ina.getRange())
	{
	case INA219_RANGE_16V:            DBGLN("16V"); break;
	case INA219_RANGE_32V:            DBGLN("32V"); break;
	default: DBGLN("unknown");
	}

	DBG("Gain:                 ");
	switch (ina.getGain())
	{
	case INA219_GAIN_40MV:            DBGLN("+/- 40mV"); break;
	case INA219_GAIN_80MV:            DBGLN("+/- 80mV"); break;
	case INA219_GAIN_160MV:           DBGLN("+/- 160mV"); break;
	case INA219_GAIN_320MV:           DBGLN("+/- 320mV"); break;
	default: DBGLN("unknown");
	}

	DBG("Bus resolution:       ");
	switch (ina.getBusRes())
	{
	case INA219_BUS_RES_9BIT:         DBGLN("9-bit"); break;
	case INA219_BUS_RES_10BIT:        DBGLN("10-bit"); break;
	case INA219_BUS_RES_11BIT:        DBGLN("11-bit"); break;
	case INA219_BUS_RES_12BIT:        DBGLN("12-bit"); break;
	default: DBGLN("unknown");
	}

	DBG("Shunt resolution:     ");
	switch (ina.getShuntRes())
	{
	case INA219_SHUNT_RES_9BIT_1S:    DBGLN("9-bit / 1 sample"); break;
	case INA219_SHUNT_RES_10BIT_1S:   DBGLN("10-bit / 1 sample"); break;
	case INA219_SHUNT_RES_11BIT_1S:   DBGLN("11-bit / 1 sample"); break;
	case INA219_SHUNT_RES_12BIT_1S:   DBGLN("12-bit / 1 sample"); break;
	case INA219_SHUNT_RES_12BIT_2S:   DBGLN("12-bit / 2 samples"); break;
	case INA219_SHUNT_RES_12BIT_4S:   DBGLN("12-bit / 4 samples"); break;
	case INA219_SHUNT_RES_12BIT_8S:   DBGLN("12-bit / 8 samples"); break;
	case INA219_SHUNT_RES_12BIT_16S:  DBGLN("12-bit / 16 samples"); break;
	case INA219_SHUNT_RES_12BIT_32S:  DBGLN("12-bit / 32 samples"); break;
	case INA219_SHUNT_RES_12BIT_64S:  DBGLN("12-bit / 64 samples"); break;
	case INA219_SHUNT_RES_12BIT_128S: DBGLN("12-bit / 128 samples"); break;
	default: DBGLN("unknown");
	}

	DBG("Max possible current: ");
	DBG(ina.getMaxPossibleCurrent());
	DBGLN(" A");

	DBG("Max current:          ");
	DBG(ina.getMaxCurrent());
	DBGLN(" A");

	DBG("Max shunt voltage:    ");
	DBG(ina.getMaxShuntVoltage());
	DBGLN(" V");

	DBG("Max power:            ");
	DBG(ina.getMaxPower());
	DBGLN(" W");
}




//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass Settings;
//--------------------------------------------------------------------------------------------------------------------------------
SettingsClass::SettingsClass()
{
	canUseBlinkerMessage  = true;
	BlinkerReadyGreen    = true;
	BlinkerReadyRed = true;
}

//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetTimeLedLCD()  // Получить время отключения подсветки дисплея при отсутсвии активности и снижении питания
{
	uint16_t led_POWER = GetLED_Power();
	uint16_t result;
	if (led_POWER < 20)
	{
		result = 5;
	}
	if ((led_POWER >= 20) && (led_POWER < 50))
	{
		result = 10;
	}
	if (led_POWER >= 50)
	{
		uint16_t corr_data = MemRead(TimeLedLCD_ADDRESS);
		result = MemReadInt(TimeLedLCD_ADDRESS + 1);
		if (corr_data != CORRECT_DATA)
		{
			result = BACKLIGHT_OFF_DELAY; // по умолчанию 
		}

		if (result < 1)
		{
			result = BACKLIGHT_OFF_DELAY;
		}
	}

    return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetTimeLedLCD(uint16_t val) // Записать время отключения подсветки дисплея при отсутсвии активности
{
    if (val < 1)
    {
        val = BACKLIGHT_OFF_DELAY;
    }
	MemWrite(TimeLedLCD_ADDRESS, CORRECT_DATA);
	MemWriteInt(TimeLedLCD_ADDRESS + 1, val);
}


//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SettingsClass::GetLED_Power()         // Получить время отключения подсветки дисплея при отсутсвии активности и снижении питания и перехода на фиксированное время отключения
{
	return led_powerOff;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetLED_Power(uint8_t val) // Записать время отключения подсветки дисплея при отсутсвии активности, снижении питания и перехода на фиксированное время отключения
{
	led_powerOff = val;
}
//
////--------------------------------------------------------------------------------------------------------------------------------
//bool SettingsClass::GetClearUSB()         // получить флаг необходимости очистить экран при зарядке от USB
//{
//	return USB_LCD_clear;
//}
////--------------------------------------------------------------------------------------------------------------------------------
//void SettingsClass::SetClearUSB(bool clear_USB) // установить флаг необходимости очистить экран при зарядке от USB
//{
//	USB_LCD_clear = clear_USB;
//}

//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetTimeAkk()           // Получить время работы аккумулятора
{
	uint16_t corr_data = MemRead(TimeAkk_ADDRESS);
	uint16_t result = MemReadInt(TimeAkk_ADDRESS + 1);
	if (corr_data != CORRECT_DATA)
	{
		result = POWER_TIME; // по умолчанию 
	}

	if (result < 1)
	{
		result = POWER_TIME;
	}
	return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetTimeAkk(uint16_t val) // Записать  время работы аккумулятора
{
	if (val < 1)
	{
		val = POWER_TIME;
	}
	MemWrite(TimeAkk_ADDRESS, CORRECT_DATA);
	MemWriteInt(TimeAkk_ADDRESS + 1, val);
}

//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetTimeReturnMainMenu()             // Получить время возврата в главное меню
{
	uint16_t corr_data = MemRead(TimeMainMenu_ADDRESS);
	uint16_t result = MemReadInt(TimeMainMenu_ADDRESS + 1);
	if (corr_data != CORRECT_DATA)
	{
		result = RESET_TO_MAIN_SCREEN_DELAY; // по умолчанию 
	}

	if (result < 1)
	{
		result = RESET_TO_MAIN_SCREEN_DELAY;
	}
	return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetTimeReturnMainMenu(uint16_t val)     // Записать время возврата в главное меню
{
	if (val < 1)
	{
		val = RESET_TO_MAIN_SCREEN_DELAY;
	}
	MemWrite(TimeMainMenu_ADDRESS, CORRECT_DATA);
	MemWriteInt(TimeMainMenu_ADDRESS + 1, val);
}

//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetPowerMinimumThreshold()             // Получить минимальный порог напряжения аккумулятора
{
	uint16_t corr_data = MemRead(Power_min_threshold_ADDRESS);
	uint16_t result = MemReadInt(Power_min_threshold_ADDRESS + 1);
	if (corr_data != CORRECT_DATA)
	{
		result = DEFAULT_POWER_LOW;                           // по умолчанию минимальный порог напряжения аккумулятора
	}

	if (result < 1)
	{
		result = DEFAULT_POWER_LOW;
	}
	return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetPowerMinimumThreshold(uint16_t val)     // Записать минимальный порог напряжения аккумулятора
{
	if (val < 1)
	{
		val = DEFAULT_POWER_LOW;                              // по умолчанию минимальный порог напряжения аккумулятора
	}
	MemWrite(Power_min_threshold_ADDRESS, CORRECT_DATA);
	MemWriteInt(Power_min_threshold_ADDRESS + 1, val);
}

//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetPowerMaximumThreshold()             // Получить максимальный порог напряжения аккумулятора
{
	uint16_t corr_data = MemRead(Power_max_threshold_ADDRESS);
	uint16_t result = MemReadInt(Power_max_threshold_ADDRESS + 1);
	if (corr_data != CORRECT_DATA)
	{
		result = DEFAULT_POWER_HIGH;                           // по умолчанию максимальный порог напряжения аккумулятора
	}

	if (result < 1)
	{
		result = DEFAULT_POWER_HIGH;
	}
	return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetPowerMaximumThreshold(uint16_t val)     // Записать максимальный порог напряжения аккумулятора
{
	if (val < 1)
	{
		val = DEFAULT_POWER_HIGH;                              // по умолчанию максимальный порог напряжения аккумулятора
	}
	MemWrite(Power_max_threshold_ADDRESS, CORRECT_DATA);
	MemWriteInt(Power_max_threshold_ADDRESS + 1, val);
}


//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetButtonRetention(bool val) // Записать
{
	ButtonRetention = val;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::displayBacklight(bool bOn)
{
    digitalWrite(KEY_POWER_12V, bOn ? HIGH : LOW);
    backlightFlag = bOn;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setLEDmenu(bool ledOn)
{
	setLEDmenuFlag = ledOn;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::setup()
{

	pinMode(KEY_POWER_12V, OUTPUT);           //  Настроить   подачу питания на преобразователь 12 вольт
	digitalWrite(KEY_POWER_12V, LOW);         //  Включить  подачу питания на преобразователь 12 вольт

	pinMode(KEY_POWER_5V, OUTPUT);             //  Настроить   подачу питания на преобразователь 5 вольт
	digitalWrite(KEY_POWER_5V, HIGH);          //  Включить  подачу питания на преобразователь 5 вольт

	pinMode(LED_GREEN_MESSAGE, OUTPUT);        // Настроить светодиод "СООБЩЕНИЕ"
	pinMode(LED_RED_MESSAGE, OUTPUT);          // Настроить светодиод "СООБЩЕНИЕ"
	pinMode(LED_GREEN_READY, OUTPUT);          // Настроить светодиод "ГОТОВ"
	pinMode(LED_RED_READY, OUTPUT);            // Настроить светодиод "ГОТОВ"
	pinMode(LED_RED_HL1, INPUT);               // Настроить светодиод HL1 красный
	pinMode(LED_GREEN_HL1, INPUT);             // Настроить светодиод HL1 красный

	pinMode(RESET_LCD, OUTPUT);                // Настроить вход сброса индикатора
	digitalWrite(RESET_LCD, HIGH);

	pinMode(KEY_LED_POINT, OUTPUT);            // Ключ включения подсветки кнопок чтения сообщений
 
	pinMode(KEY_LED_SOS, OUTPUT);              // Ключ включения подсветки кнопок чтения сообщений

	analogWrite(KEY_LED_SOS, 0);               // Ключ управления подсветкой кнопок ШИМ сигналом
	analogWrite(KEY_LED_POINT, 0);             // Ключ включения подсветки кнопок 

	pinMode(BUTTON_SOS, INPUT_PULLUP);         // Настроить кнопку "SOS"
	pinMode(BUTTON_POINT, INPUT_PULLUP);       // Настроить кнопку "POINT"
	pinMode(BUTTON_LEFT, INPUT_PULLUP);        // Настроить кнопку "<"
	pinMode(BUTTON_RIGHT, INPUT_PULLUP);       // Настроить кнопку ">"
	pinMode(GPS_GREEN_LED, INPUT_PULLDOWN);    // Вход определения наличия связи с GPS. Подключен к светодиодам индикации обмена с GPS
	pinMode(HL4_L_POINT, INPUT_PULLDOWN);      // Индикация светодиодом "Режим POINT"
	pinMode(HL2_L_SOS, INPUT_PULLDOWN);        // Индикация светодиодом "Режим SOS"
	pinMode(POWER_BATTERY, INPUT);             // Настроить pin измерения напряжения аккумулятора

	reset_LCD();                               // Выполнить сброс дисплея

	analogReadResolution(12);
	setLedConnectBase(false);                  // Установить флаг подключения к базе IRIDIUM (для индикации диодом "Готов")	
  

	setMessageDiodeBlink(false);               // Запретить мигание светодиода "Сообщение" (красный)   
	setReadyDiodeGreenBlink(false);            // Запретить мигание светодиода "Готов" (зеленый)   
	setReadyDiodeRedBlink(false);              // Запретить мигание светодиода "Готов" (красный)   
	setReadyDiodeAllBlink(false);              //
	displayViewSOS(false);
	displayViewPOINT(true);

	setLEDmenu(false);                         // Флаг выбора режима работы светодиодов подсветки кнопок

	SAT_Button.begin(LED_GREEN_HL1, false, SAT_GSM_TIME_ON);   // Настроить контроль функции определения связи с Iridium
	LED_SOS_Button.begin(HL2_L_SOS, false, 1000);   // Настроить контроль функции определения SOS


	/* Определяем тип установленного датчика света*/
	num_opt_deviceN = Settings.getOptDevice();
	delay(10);
	if (num_opt_deviceN == 1)
	{
		lightMeterOPT3001.begin(OPT3001_ADDRESS);
		OPT3001_Config newConfig;

		newConfig.RangeNumber = B1100;
		newConfig.ConvertionTime = B0;
		newConfig.Latch = B1;
		newConfig.ModeOfConversionOperation = B11;

		OPT3001_ErrorCode errorConfig = lightMeterOPT3001.writeConfig(newConfig);
		if (errorConfig != NO_ERROR)
		{
			//printError("OPT3001 configuration", errorConfig); // Пока не применяем. 
		}
		else
		{
			OPT3001_Config sensorConfig = lightMeterOPT3001.readConfig();
		}
		delay(10);
	}
	else if (num_opt_deviceN == 2)
	{
		lightMeterBH1750.begin();
	}


	//DBGLN("Initialize INA219");
	//DBGLN("-----------------------------------------------");

	// Default INA219 address is 0x40
	ina.begin();

	// Configure INA219
	ina.configure(INA219_RANGE_16V, INA219_GAIN_320MV, INA219_BUS_RES_12BIT, INA219_SHUNT_RES_12BIT_1S);

	// Calibrate INA219. Rshunt = 0.1 ohm, Max excepted current = 2A
	ina.calibrate(0.1, 2);

	// Display configuration
	checkConfig();

	//DBGLN("-----------------------------------------------");

	/* Очищаем массив измерения напряжения аккумулятора*/
	delay(TIME_TO_SETTLE_DOWN * 1000);                   // Немного подождать для успокоения питания
	for (int i = 0; i < array_size; i++)
	{
		dimension_array[i] = 0;
	}

	/* Инициализируем сторожевой таймер*/
#ifdef USE_WATCHDOG_TIMER 
	MX_IWDG_Init();                                      // Инициализация сторожевого таймера 
	HAL_IWDG_Refresh(&hiwdg);                            // Сброс сторожевого таймера
#endif

	test_Led();                                          // Тестируем диоды на лицевой панели

}

//--------------------------------------------------------------------------------------------------------------------------------

void SettingsClass::MX_IWDG_Init()
{
#ifdef USE_WATCHDOG_TIMER 
	hiwdg.Instance = IWDG;
	hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
	hiwdg.Init.Reload = 4800;
	if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
	{
		//Error_Handler();
	}
#endif
}

void SettingsClass::reset_IWDG()
{
#ifdef USE_WATCHDOG_TIMER 
	HAL_IWDG_Refresh(&hiwdg);                // Сброс сторожевого таймера
#endif
}

void SettingsClass::test_Led()               // Тестируем диоды на лицевой панели
{
    digitalWrite(LED_GREEN_MESSAGE, HIGH);
	delay(100);
	digitalWrite(LED_GREEN_MESSAGE, LOW);
	delay(100);
    digitalWrite(LED_RED_MESSAGE, HIGH);
	delay(100);
	digitalWrite(LED_RED_MESSAGE, LOW);
	delay(100);
	digitalWrite(LED_GREEN_READY, HIGH);
	delay(100);
	digitalWrite(LED_GREEN_READY, LOW);
	delay(100);
	digitalWrite(LED_RED_READY, HIGH);
	delay(100);
	digitalWrite(LED_RED_READY, LOW);
	delay(100);

	setReadyDiodeAllBlink(true);                                    // Разрешить мигание светодиода "Готов"
}

void SettingsClass::update()
{
	#ifdef USE_WATCHDOG_TIMER 
	Settings.reset_IWDG();      // Сброс сторожевого таймера // Сброс сторожевого таймера
    #endif

	/* Проверяем подключение к Iridium*/
	SAT_Button.update();
	if (SAT_Button.isRetention())
	{
		SAT_on = true;
		displayConnectBase(SAT_on);
		setReadyDiodeAllBlink(false);                     // Запретить мигание светодиода "Готов" (красный и зеленый)   
	}

	/* Определяем подключение к разъему USB */
	static uint32_t tmrUSB = millis();
	byte USB_connect = digitalRead(POWER_USB);

//	DBG("USB_connect");
//	DBGLN(USB_connect);

	if (USB_connect /*USB_connect > 100*/)                // Просто определяем уровень на входе USB
	{
		USB_on_tmp = true;
	}
	else
	{
		USB_on_tmp = false;
		tmrUSB = millis();
	}

	if ((millis() - tmrUSB > USB_TIME_ON) && (USB_on_tmp))
	{
		//DBGLN("USB on");
		setConnectUSB(true);
	}
	else
	{
		//DBGLN("USB off");
		setConnectUSB(false);
	}

	static uint32_t tmrGPS = millis();
	bool GPS_connect = Settings.get_LED_GPS();

	if (GPS_connect == true)                    // Просто определяем уровень на входе GPS
	{
		GPS_on_tmp = true;
		//DBGLN("GPS on");
		tmrGPS = millis();
		GPS_on = false;
		Settings.set_LED_GPS(false);
	}
	else
	{
		GPS_on_tmp = false;
	}

	if ((millis() - tmrGPS > GPS_TIME_OFF) && (!GPS_on_tmp))
	{
		tmrGPS = millis();
		//DEBUG_Serial.print("GPS off -");
		Settings.set_LED_GPS(false);
		GPS_on = true;
	}
	else
	{
		GPS_on = false;
		GPS_on_tmp = true;
	}

	/* Контроль светодиода SOS*/


	LED_SOS_Button.update();
	if (LED_SOS_Button.isDoubleClicked())
	{
		SOS_button_tmp = true;
	}


	/* Контроль датчика света */
	lcd_ON_Flag = Settings.isBacklightOn();               // получить состояние дисплея, включен или нет
	bool LCDset_menuOn = isLEDmenuOn();
	if (!LCDset_menuOn)
	{
		if (lcd_ON_Flag)
		{
			static uint32_t tmr = millis();

			if (millis() - tmr > 300)
			{
				if (num_opt_deviceN == 1)
				{

					OPT3001 luxAll = lightMeterOPT3001.readResult();
					lux = luxAll.lux;
				}
				else if (num_opt_deviceN == 2)
				{
					if (lightMeterBH1750.measurementReady(true))
					{
						lux = lightMeterBH1750.readLightLevel();
					}
				}

				int val = Settings.getLED_Brightness();
				int val_lux = Settings.getLuxOptModule();

				//val = map(lux, 0, MAX_LUX_OPT_MODULE, 2, 255);
				//if (val > 255) val = 255;

				if (lux < val_lux)
				{
					analogWrite(KEY_LED_SOS, val);                // Ключ управления подсветкой кнопок ШИМ сигналом
					analogWrite(KEY_LED_POINT, val);              // Ключ включения подсветки кнопок 
				}
				else
				{
					analogWrite(KEY_LED_SOS, 0);                        // Ключ управления подсветкой кнопок ШИМ сигналом
					analogWrite(KEY_LED_POINT, 0);                      // Ключ включения подсветки кнопок 
				}
				DBG(F("Light: "));
				DBG(lux);
				DBG(F(" lx "));
				DBGLN(val);

				tmr = millis();
			}
		}
		else
		{
			analogWrite(KEY_LED_SOS, 0);                        // Ключ управления подсветкой кнопок ШИМ сигналом
			analogWrite(KEY_LED_POINT, 0);                      // Ключ включения подсветки кнопок 
		}
	}

	bool allLedBlink = Settings.getReadyDiodeAllBlink();        // получить состояние мигание светодиода "Готов" (красный и зеленый) 
	bool flight = Settings.getButtonBlock();                    // получить состояние блокировки кнопок

	if (!flight)                                               // Если кнопки не заблокированы после завершения полета.
	{
		if (allLedBlink)
		{
			static uint32_t tmr = millis();

			if (millis() - tmr > READY_DIODE_BLINK_INTERVAL)
			{
				tmr = millis();

				if (ledState == LOW)
				{
					ledState = HIGH;
				}
				else
				{
					ledState = LOW;
				}

				if (BlinkerReadyGreen)
				{
					digitalWrite(LED_GREEN_READY, ledState);
					digitalWrite(LED_RED_READY, LOW);
				}
				else if (BlinkerReadyRed)
				{
					digitalWrite(LED_RED_READY, ledState);
					digitalWrite(LED_GREEN_READY, LOW);
				}
				else if (BlinkerReadyOrange)
				{
					digitalWrite(LED_GREEN_READY, ledState);
					digitalWrite(LED_RED_READY, ledState);
				}
			}
		}
	}
	else
	{
		digitalWrite(LED_RED_READY, LOW);
		digitalWrite(LED_GREEN_READY, LOW);
		analogWrite(KEY_LED_SOS, 0);                        // Ключ управления подсветкой кнопок ШИМ сигналом
		analogWrite(KEY_LED_POINT, 0);                      // Ключ включения подсветки кнопок 
	}

	bool MessLedBlink = Settings.getMessageDiodeBlink();           // получить состояние мигание светодиода "Сообщение" (красный) 

	if (MessLedBlink)
	{
		static uint32_t tmrMss = millis();

		if (millis() - tmrMss > MESSAGE_DIODE_BLINK_INTERVAL)
		{
			tmrMss = millis();

			if (ledStateMess == LOW)
			{
				ledStateMess = HIGH;
			}
			else
			{
				ledStateMess = LOW;
			}
			digitalWrite(LED_RED_MESSAGE, ledStateMess);
		}
	}
	else
	{
		digitalWrite(LED_RED_MESSAGE, LOW);
	}
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::reset_LCD()                         // Выполнить сброс индикатора
{
    digitalWrite(RESET_LCD, HIGH);
    delay(20);
    digitalWrite(RESET_LCD, LOW);
    delay(20);
    digitalWrite(RESET_LCD, HIGH);
}
//--------------------------------------------------------------------------------------------------------------------------------

byte  SettingsClass::getOptDevice()
{
	return num_opt_device;
}

byte  SettingsClass::setOptDevice(byte device)
{
	num_opt_device = device;
}

//--------------------------------------------------------------------------------------------------------------------------------

bool SettingsClass::getMessageDiodeBlink()
{
	canUseBlinkerMessage = MemRead(MESSAGE_LED_CONFIRMED_flag); // получить состояние светодиода нового сообщения
    return canUseBlinkerMessage;
}

void SettingsClass::setMessageDiodeBlink(bool blink)
{
 //MESSAGE_LED_CONFIRMED_flag                                  // адрес флага нового сообщения
	MemWrite(MESSAGE_LED_CONFIRMED_flag, blink);               // сохранить состояние светодиода нового сообщения
    canUseBlinkerMessage = blink;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getReadyDiodeGreenBlink()
{
    return BlinkerReadyGreen;
}

void SettingsClass::setReadyDiodeGreenBlink(bool blink)
{
	BlinkerReadyGreen = blink;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getReadyDiodeRedBlink()
{
	return BlinkerReadyRed;
}

void SettingsClass::setReadyDiodeRedBlink(bool blink)
{
	BlinkerReadyRed = blink;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getReadyDiodeOrangeBlink()
{
	return BlinkerReadyOrange;
}

void SettingsClass::setReadyDiodeOrangeBlink(bool blink)
{
	BlinkerReadyOrange = blink;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getReadyDiodeAllBlink()
{
	return BlinkerReadyAll;
}

void SettingsClass::setReadyDiodeAllBlink(bool blink)
{
	BlinkerReadyAll = blink;
}
//--------------------------------------------------------------------------------------------------------------------------------
bool  SettingsClass::getButtonBlock()
{
	return BlockButton;
}

void SettingsClass::setButtonBlock(bool block)
{
	BlockButton = block;
}
//--------------------------------------------------------------------------------------------------------------------------------

bool  SettingsClass::get_empty_buffer_request()
{
	return empty_buffer;
}

void SettingsClass::set_empty_buffer_request(bool buffer_request)
{
	empty_buffer = buffer_request;
}


//--------------------------------------------------------------------------------------------------------------------------------
// Работа с записями в энергонезависимой памяти
//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::GetMemorySize()             // Получить максимальный размер применяемой памяти
{
	uint16_t corr_data = MemRead(MAX_MEMORY_SIZE_ADDRESS);
	uint16_t result = MemReadInt(MAX_MEMORY_SIZE_ADDRESS + 1);
	if (corr_data != CORRECT_DATA)
	{
		result = MAX_MEMORY_SIZE; // по умолчанию 
	}

	if (result < 1)
	{
		result = MAX_MEMORY_SIZE;
	}
	return result;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetMemorySize(uint16_t val)     // Записать максимальный размер применяемой памяти
{
	if (val < 1)
	{
		val = MAX_MEMORY_SIZE;
	}
	MemWrite(MAX_MEMORY_SIZE_ADDRESS, CORRECT_DATA);
	MemWriteInt(MAX_MEMORY_SIZE_ADDRESS + 1, val);
}

//1
//--------------------------------------------------------------------------------------------------------------------------------
/* Получить состояние счетчика не подтвержденных сообщений */
uint8_t SettingsClass::getCoutNotReadMessage()
{
	// Получить количество полученных сообщений
	uint8_t mess_count = MemRead(Count_NotRead_Message_ADDRESS); // адрес хранения счетчика не прочитанного количества записей
	return mess_count;
}

/* Сохранить в памяти количество не подтвержденных сообщений */
void SettingsClass::setCoutNotReadMessage(uint8_t count)
{
	// Сохранить количество полученных сообщений
	MemWrite(Count_NotRead_Message_ADDRESS, count);              // сохранить состояние счетчика не прочитанного количества записей 
}

//2
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getCurrentCountMessage()
{
    // Получить текущий номер сообщения
    uint8_t current_count_mess = MemRead(Current_Counter_Message); // получить состояние текущего счетчика
    return current_count_mess;
}

void SettingsClass::setCurrentCountMessage(uint8_t count_cur)
{
    // Сохранить текущий номер сообщения
    MemWrite(Current_Counter_Message, count_cur);      // сохранить текущее состояние счетчика
}

//3
//--------------------------------------------------------------------------------------------------------------------------------
/*  Программы поддержки функции листания сообщений*/
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t SettingsClass::getFlippingCountMessage()
{
    // Получить текущий номер сообщения
    uint8_t current_count_mess = MemRead(Flipping_Counter_Message); // получить текущее состояние положения счетчика
    return current_count_mess;
}

void SettingsClass::setFlippingCountMessage(uint8_t count_cur)
{
    // Сохранить текущий номер сообщения
    MemWrite(Flipping_Counter_Message, count_cur);      // сохранить текущее положение счетчика
}

//--------------------------------------------------------------------------------------------------------------------------------
uint8_t  SettingsClass::getLED_Brightness()
{
	uint16_t corr_data = MemRead(LED_BRIGHTNESS_ADDRESS);
	uint8_t brightness = MemRead(LED_BRIGHTNESS_ADDRESS + 1); // получить временное 
	if (corr_data != CORRECT_DATA)
	{
		brightness = DEFAULT_LED_BRIGHTNESS; // по умолчанию 
	}

	if (brightness < 2)
	{
		brightness = MIN_LED_BRIGHTNESS;
	}
	if (brightness > 255)
	{
		brightness = MAX_LED_BRIGHTNESS;
	}
	return brightness;
}

void SettingsClass::setLED_Brightness(uint8_t brightness)
{
	if (brightness < 2)
	{
		brightness = MIN_LED_BRIGHTNESS;
	}
	if (brightness > 255)
	{
		brightness = MAX_LED_BRIGHTNESS;
	}
	MemWrite(LED_BRIGHTNESS_ADDRESS, CORRECT_DATA);
	MemWrite(LED_BRIGHTNESS_ADDRESS + 1, brightness);
}


//--------------------------------------------------------------------------------------------------------------------------------
uint16_t  SettingsClass::getLuxOptModule()
{
	unsigned int corr_data = MemRead(LUX_OPT_ADDRESS);
	unsigned int opt_val = MemReadInt(LUX_OPT_ADDRESS+1);  // адрес хранения листания текстовых сообщений
	if (corr_data != CORRECT_DATA)
	{
		opt_val = DEFAULT_LUX_OPT_MODULE; // по умолчанию 
	}
	return opt_val;
}

void SettingsClass::setLuxOptModule(uint16_t val_lux)// адрес хранения минимального уровня датчика света
{
	MemWrite(LUX_OPT_ADDRESS, CORRECT_DATA);
	MemWriteInt(LUX_OPT_ADDRESS + 1, val_lux);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getNewMessageFlag()
{
	// Получить текущий номер сообщения
	bool new_flag = MemRead(MESSAGE_NOT_CONFIRMED_flag); // получить флаг нового сообщения
	return new_flag;
}

void SettingsClass::setNewMessageFlag(bool new_flag)
{
	// Сохранить текущий номер сообщения
	MemWrite(MESSAGE_NOT_CONFIRMED_flag, new_flag);      // сохранить флаг нового сообщения
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getCompletion_flight()
{
	// Получить текущий номер сообщения
	bool new_flag = MemRead(SUCCESSFUL_OF_THE_FLIGHT_ADR); // получить флаг нового сообщения
	return new_flag;
}

void SettingsClass::setCompletion_flight(bool new_flag)
{
	// Сохранить текущий номер сообщения
	MemWrite(SUCCESSFUL_OF_THE_FLIGHT_ADR, new_flag);      // сохранить флаг нового сообщения
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::checkConnectUSB() // Определение сеанса связи с базой
{
	return USB_on;
}

void SettingsClass::setConnectUSB(bool USB_level)                       // Установить флаг подключения к разъему USB
{
	USB_on = USB_level;
}


//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::GetClearUSB()         // получить флаг необходимости очистить экран при зарядке от USB
{
	return USB_LCD_clear;
}
//--------------------------------------------------------------------------------------------------------------------------------
void SettingsClass::SetClearUSB(bool clear_USB) // установить флаг необходимости очистить экран при зарядке от USB
{
	USB_LCD_clear = clear_USB;
}



//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::checkConnectBase() // Определение сеанса связи с базой
{
	return SAT_on;
}

void SettingsClass::displayConnectBase(bool satOn)                       // Установить флаг подключения к базе IRIDIUM 
{
	SAT_on = satOn;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SettingsClass::getledConnectBase() // Определение сеанса связи с базой (для индикации диодом "Готов")
{
	return connect_base;
}

void SettingsClass::setLedConnectBase(bool satOn)                       // Установить флаг подключения к базе IRIDIUM (для индикации диодом "Готов") 
{
	connect_base = satOn;
}

//--------------------------------------------------------------------------------------------------------------------------------

bool SettingsClass::checkModeSOS()                                       // Определение режима SOS
{

	return SOS_on;
}

void SettingsClass::displayViewSOS(bool SOSOn)                           // Установить флаг режима SOS
{
	if(SOS_button_tmp)
	SOS_on = SOSOn;
}

bool SettingsClass::checkModePOINT()                                     // Определение режима POINT
{
	return POINT_on;
}

void SettingsClass::displayViewPOINT(bool POINTOn)                       // Установить флаг режима POINT
{
	POINT_on = POINTOn;
}

bool SettingsClass::get_LED_GPS()                                       //  Определение режима отключения прибора
{
	return GPS_LED_on;
}
void SettingsClass::set_LED_GPS(bool led_on)                           //  Определение режима отключения прибора
{
	GPS_LED_on = led_on;
}

void  SettingsClass::set_LED_GPS_status(bool led_status)               // Определение длительного отключения светодиода GPS
{
	GPS_on = led_status;
}              
//-------------------------------------------------------------------------------------------------------------

bool SettingsClass::get_checkDeviceOff()                                // Определение режима отключения прибора
{
	return device_off;
}


void SettingsClass::set_checkDeviceOff(uint8_t led_on)                 //  Определение режима отключения прибора
{
	//DBG("led_on - ");
	//DBGLN(led_on);

	switch (led_on)
	{
		/*      */
		case 0:
			//выполняется, когда var равно 0
			HL4_L_POINT_on = false;
			HL2_L_SOS_on = false;
			LED_GREEN_HL1_on = false;
			GPS_GREEN_LED_on = false;
			device_off = false;
			//DBGLN("off");
			break;
		case 1:
			//выполняется, когда var равно 1
			HL2_L_SOS_on = true;
			//DBGLN("SOS");
			break;
		case 2:
			//выполняется когда  var равно 2
			HL4_L_POINT_on = true;
			//DBGLN("POINT");
			break;
		case 3:
			//выполняется, когда var равно 3
			LED_GREEN_HL1_on = true;
			//DBGLN("SAT");
			break;
		case 4:
			//выполняется, когда var равно 4
			GPS_GREEN_LED_on = true;
			//DBGLN("GPS");
			break;
		default:
			break;
			// выполняется, если не выбрана ни одна альтернатива
			// default необязателен
	}

	if (HL4_L_POINT_on && HL2_L_SOS_on && LED_GREEN_HL1_on && GPS_GREEN_LED_on)
	{
		device_off = true;
	}
	else
	{
		device_off = false;
	}

	if (led_on == 4)
	{
	   // DBGLN("GPS LED on");
		Settings.set_LED_GPS(true);       // Установить флаг GPS в работе
		bool flight = Settings.getButtonBlock();
		if (flight)
		{
			Settings.displayBacklight(true);
			//backlightFlag = true;
		}
		Settings.setButtonBlock(false);   // Разблокировать кнопки.
	}

	if (HL2_L_SOS_on && !HL4_L_POINT_on)
	{
		Settings.displayViewSOS(true);                            // Установить флаг режима SOS
		Settings.displayViewPOINT(false);                         // Установить флаг режима POINT
	}

	if (!HL2_L_SOS_on && HL4_L_POINT_on)
	{
		Settings.displayViewSOS(false);                           // Установить флаг режима SOS
		Settings.displayViewPOINT(true);                          // Установить флаг режима POINT
	}


}
//--------------------------------------------------------------------------------------------------------------------------------
uint16_t SettingsClass::getPowerVoltageAkk(uint16_t pin) // Контроль напряжения питания внутренних источников (аккумуляторов).
{


	float ina_voltage = ina.readBusVoltage();
	voltageAkk1 = ina_voltage * 100;

	float BusPower = ina.readBusPower();

	//float ShuntVoltage = ina.readShuntVoltage();

	//float ShuntCurrent = ina.readShuntCurrent();

	//DBG("Bus voltage:   ");
	//DBG(ina_voltage);
	//DBGLN(" V");

	//DBG("Bus power:     ");
	//DBG(BusPower);
	//DBGLN(" W");

	///*
	//DBG("Shunt voltage: ");
	//Serial.print(ina.readShuntVoltage(), 5); 
	//DBGLN(" V");*/

	//DBG("Shunt current: ");
	//Serial.print(ina.readShuntCurrent(), 5);
	//DBGLN(" A");

	//DBGLN("");

	dimension_array[array_count] = voltageAkk1;
	array_count++;
	int val_voltage = 0;
	if (array_count > array_size)                    // проверка заполнения массива первичными данными о уровне напряжения аккумулятора
	{
		array_count = 0;
		array_countMax = true;                       //Разрешить выдавать данные об уровне напряжения аккумулятора
	}

	sum = 0;                                         //

	if (array_countMax)                              // формируем данные об уровне напряжения аккумулятора
	{
		for (int i = 0; i < array_size; i++)
		{
			sum += dimension_array[i];
		}
		val_voltage = sum / array_size;
	}
	else
	{
		for (int i = 0; i < array_count; i++)       //формируем первичные (заполняем массив) данные об уровне напряжения аккумулятора
		{
			sum += dimension_array[array_count-1];
		}
		val_voltage = sum / array_count;
	}

	sum = 0;
	DBG(F("val  "));
	DBGLN(val_voltage);
	return val_voltage;                                 //Напряжение питания аккумулятора
}

//--------------------------------------------------------------------------------------------------------------------------------
