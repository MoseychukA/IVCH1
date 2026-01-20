#include "WeatherStation.h"
#include "LogicManageModule.h"
#include "EEPROMSettingsModule.h"
#include "AbstractModule.h"


// Библиотека iarduino_I2C_connect разработана для удобства соединения нескольких arduino по шине I2C
// Данная Arduino является ведущим устройством на шине I2C

// Подключаем библиотеки:
#include <Wire.h>                     // подключаем библиотеку для работы с шиной I2C
#include <iarduino_I2C_connect.h>     // подключаем библиотеку для соединения arduino по шине I2C

// Объявляем переменные и константы:
iarduino_I2C_connect WeaterI2C2;            // объявляем переменную для работы c библиотекой iarduino_I2C_connect

byte stationID = 0;           // ID станции
byte crc = 0;                 // Принятая контрольная сумма пакета
byte CRC_Calc = 0;            // Расчетная контрольная суммв пакета
int tempI2C = 0;              // Температура воздуха
byte humI2C = 0;              // Влажность воздуха
int windI2C = 0;              // Скорость ветра
int gust_wind = 0;            // Порыв ветра             
byte wind_dirI2C = 0;         // Направление ветра
byte rainI2C = 0;             // Счетчик дождя
byte REG_Array1[20];          // объявляем массив, данные которого будут доступны для чтения/записи по шине I2C

/*
Пакет MISOL WS0232
REG_Array1[0];                     // ID станции   
REG_Array1[1];                     // 
REG_Array1[2];                     //
REG_Array1[3];                     //
REG_Array1[4];                     // Старший байт значения температуры ведомого (адрес ведомого 0x01, номер регистра 1) + направление ветра
REG_Array1[5];                     // Младший байт значения температуры
REG_Array1[6];                     // Влажность воздуха
REG_Array1[7];                     // Старший байт скорости ветра
REG_Array1[8];                     // Младший байт скорости ветра
REG_Array1[9];                     // 
REG_Array1[10];                    //
REG_Array1[11];                    // Передает состояние счетчика импульсов коромысла.
REG_Array1[12];                    // принятую контрольную сумму из пакета
REG_Array1[13];                    // контрольную сумму расчитанную пакета
REG_Array1[14];                    // Отправляем состояние флага ведомому (адрес ведомого 0x01, номер регистра 14, состояние флага)
REG_Array1[15] = 0;                // Записать тип применяемой метеостанции
REG_Array1[16] = 0;                // Проверка подключения к приемнику метеостанции

//==========================================================================================================
Пакет MISOL WN5300CA

REG_Array1[0] = SecurityCode;      // ID метеостанции. Код безопасности
REG_Array1[1] = TempBitError;      // ошибки показаний температуры
REG_Array1[2] = Temperatue >> 8;   // Старший байт значения температуры ведомого (адрес ведомого 0x01, номер регистра 1) + направление ветра
REG_Array1[3] = Temperatue;        // Младший байт значения температуры
REG_Array1[4] = Humidity;          // Влажность воздуха
REG_Array1[5] = WindSpeed;         // Скорость ветра
REG_Array1[6] = Gust;              // Порывы ветра
REG_Array1[7] = RainCounter >> 8;  // Старший байт счётчик осадков
REG_Array1[8] = RainCounter;       // Младший байт счётчик осадков
REG_Array1[9] = WindDirBitError;   // Бит ошибки показаний направления ветра
REG_Array1[10] = LowBattBit;       // Бит разряженной батареи передатчика
REG_Array1[11] = WindDir;          // Направление ветра
REG_Array1[12] = calc_REG_Array(); // Записать расчетную контрольную сумму в массив
REG_Array1[13] = 0;                //
REG_Array1[14] = true;             // Записать подтверждение готовности пакета к передаче
REG_Array1[15] = 0;                // Записать тип применяемой метеостанции
REG_Array1[16] = 0;                // Проверка подключения к приемнику метеостанции

*/

float temp = 0;
unsigned long previousMillis = 0;        // will store last time LED was updated
uint32_t  readTimeoutMisol = 120000;     // Отсутствие информации от станции в течении 2 минут

//--------------------------------------------------------------------------------------------------------------------------------------
WeatherStationClass WeatherStation;
//--------------------------------------------------------------------------------------------------------------------------------------
uint8_t flag = 0;
uint8_t lastRainPulse = 0xFF;
//--------------------------------------------------------------------------------------------------------------------------------------
WeatherStationClass::WeatherStationClass()
{
 
}
//--------------------------------------------------------------------------------------------------------------------------------------
void WeatherStationClass::setup_WS0232(int16_t _ID_Misol)
{
	if (_ID_Misol != 255) 
	{
		_ID_Misol_WS0232 = _ID_Misol;
	}

	for (int i = 0; i < 20; i++)
	{
		REG_Array1[i] = 0;        // Очистить массив данных
	}


	SysDebug debug = HardwareBinding->GetSysDebug();
	WindSensorBinding bnd = HardwareBinding->GetWindSensorBinding();

	#ifdef MISOL_DEBUG

		if (debug.MISOL_DEBUG_K == HIGH)
		{
			 Serial4.print("Mode Misol ");
			 Serial4.println(bnd.WorkMode);
		}
	#endif

   Humidity = NO_TEMPERATURE_DATA;
   HumidityDecimal = 0;

   Temperature = NO_TEMPERATURE_DATA;
   TemperatureDecimal = 0;
   REG_Array1[15] = bnd.WorkMode;                // Записать тип применяемой метеостанции

   Wire.begin();
 
   currentMillis = millis();

	#ifdef MISOL_DEBUG
	
	   if (debug.MISOL_DEBUG_K == HIGH)
	   {
		    Serial4.println("Misol WS0232 setup!");
		    Serial4.print("Set Misol ID = ");
		    Serial4.println(_ID_Misol_WS0232); 
	   }
	#endif

	WeaterI2C2.writeByte(0x01, 15, REG_Array1[15]);            // Отправляем тип применяемой метеостанции приемнику метеостанции (адрес ведомого 0x01, номер регистра 15, тип метеостанции)
	WeaterI2C2.writeByte(0x01, 16, connectReceiverMisol);
	delay(100);
	byte I2C_Ok = WeaterI2C2.readByte(0x01, 16);               // Считываем состояние флага наличия подключения приемника метеостанции

	#ifdef MISOL_DEBUG
		if (debug.MISOL_DEBUG_K == HIGH)
		{
			 Serial4.print("ConnectI2C I2C_Ok =>");
			 Serial4.println(connectReceiverMisol);
			 Serial4.print(" <= ");
			 Serial4.println(I2C_Ok);
		}
	#endif
	if (I2C_Ok == connectReceiverMisol)
	{
		connectI2C_Ok = true;
		#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("ConnectI2C sucesful!");
			}
		#endif
	}
	else
	{
		connectI2C_Ok = false;
		#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("ConnectI2C failure!");
			}
		#endif
	}

	WeatherStation.SetConnectReceiver(connectI2C_Ok);
}
//--------------------------------------------------------------------------------------------------------------------------------------
void WeatherStationClass::setup_WN5300CA(int16_t _ID_Misol)
{
	if (_ID_Misol != 255)
	{
		_ID_Misol_WN5300CA = _ID_Misol;
	}

	for (int i = 0; i < 20; i++)
	{
		REG_Array1[i] = 0;        // Очистить массив данных
	}

	WindSensorBinding bnd = HardwareBinding->GetWindSensorBinding();
	SysDebug debug = HardwareBinding->GetSysDebug();

	Humidity = NO_TEMPERATURE_DATA;
	HumidityDecimal = 0;

	Temperature = NO_TEMPERATURE_DATA;
	TemperatureDecimal = 0;
	REG_Array1[15] = bnd.WorkMode;                // Записать тип применяемой метеостанции
	Wire.begin();

	currentMillis = millis();

    #ifdef MISOL_DEBUG
	    if (debug.MISOL_DEBUG_K == HIGH)
		{
			 Serial4.println("Misol WN5300CA setup!");
			 Serial4.print("Set Misol ID - ");
			 Serial4.println(_ID_Misol_WN5300CA);
		}
    #endif

		WeaterI2C2.writeByte(0x01, 15, REG_Array1[15]);            // Отправляем тип применяемой метеостанции приемнику метеостанции (адрес ведомого 0x01, номер регистра 15, тип метеостанции)
		WeaterI2C2.writeByte(0x01, 16, connectReceiverMisol);
		delay(100);
		byte I2C_Ok = WeaterI2C2.readByte(0x01, 16);               // Считываем состояние флага наличия подключения приемника метеостанции

	#ifdef MISOL_DEBUG
		if (debug.MISOL_DEBUG_K == HIGH)
		{
			 Serial4.print("ConnectI2C I2C_Ok =>");
			 Serial4.println(connectReceiverMisol);
			 Serial4.print(" <= ");
			 Serial4.println(I2C_Ok);
		}
	#endif

	if (I2C_Ok == connectReceiverMisol)
	{
		connectI2C_Ok = true;
		#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("ConnectI2C sucesful!");
			}
		#endif
	}
	else
	{
		connectI2C_Ok = false;
		#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("ConnectI2C failure!");
			}
		#endif
	}

	WeatherStation.SetConnectReceiver(connectI2C_Ok);

}
//--------------------------------------------------------------------------------------------------------------------------------------
void WeatherStationClass::update()
{
	SysDebug debug = HardwareBinding->GetSysDebug();
	WeaterI2C2.writeByte(0x01, 16, connectReceiverMisol);        // Отправляем флаг контроля подключения приемника метеостанции           
	count_var = 0;
	do
	{
		delay(15);
		I2C_Ok = WeaterI2C2.readByte(0x01, 16);                 // Считываем состояние флага наличия подключения приемника метеостанции
		if (I2C_Ok == connectReceiverMisol)                     // Приемник обнаружен - прекращаем поиск 
		{
			#ifdef MISOL_DEBUG
				if (debug.MISOL_DEBUG_K == HIGH)
				{
					 Serial4.println("ConnectI2C sucesful!");
					 Serial4.print("count_var = ");
					 Serial4.println(count_var);
				}
			#endif
			break;
		}
	    count_var++;

	} while (count_var < 10);



#ifdef MISOL_DEBUG
	if (debug.MISOL_DEBUG_K == HIGH)
	{
		 Serial4.print("ConnectI2C I2C_Ok =>");
		 Serial4.println(connectReceiverMisol);
		 Serial4.print(" <= ");
		 Serial4.println(I2C_Ok);
	}
#endif


	if (I2C_Ok == connectReceiverMisol)
	{
		connectI2C_Ok = true;                  // Приемник Misol обнаружен

#ifdef MISOL_DEBUG
		if (debug.MISOL_DEBUG_K == HIGH)
		{
			// Serial4.println("ConnectI2C sucesful!");
		}
#endif
	}
	else
	{
		connectI2C_Ok = false;                           // Приемник Misol не обнаружен
		#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("ConnectI2C failure!");
			}
		#endif
	}

	WeatherStation.SetConnectReceiver(connectI2C_Ok);

	if (connectI2C_Ok)
	{
		WeaterI2C2.writeByte(0x01, 15, REG_Array1[15]);           // Отправляем тип применяемой метеостанции приемнику метеостанции (адрес ведомого 0x01, номер регистра 15, тип метеостанции)
		delay(20);
		flag = WeaterI2C2.readByte(0x01, 14);                     // Считываем состояние флага наличия новых данных
	}


	currentMillis = millis();
	if (currentMillis - previousMillis >= readTimeoutMisol)      // контролируем наличие данных с метеостанции в течении 2 минут.
	{
		previousMillis = currentMillis;
		SetDataReceive(false);                                   // Новые данные не получили за указанный интервал
		SetDataReceived(false);                                  // Сбросить приход новых данных в указанный интервал
		Humidity = NO_TEMPERATURE_DATA;
		HumidityDecimal = 0;

		Temperature = NO_TEMPERATURE_DATA;
		TemperatureDecimal = 0;

		uint32_t windSpeed = 0;
		// просим модуль логики сохранить данные по скорости ветра
		LogicManageModule->SetWindSpeed(windSpeed);

		CompassPoints windDirection = cpUnknown;

     #ifdef MISOL_DEBUG
		if (debug.MISOL_DEBUG_K == HIGH)
		{
			 Serial4.print(F("Humidity = "));
			 Serial4.println(Humidity);
			 Serial4.print(F("Temperature = "));
			 Serial4.println(Temperature);
		}
      #endif


	} // if


	if (flag == 1) //если были данные
	{
		SetDataReceive(true);                   // Новые данные получили за указанный интервал
		SetDataReceived(true);                  // Сбросить флаг прихода стабильных данных в указанный интервал
		previousMillis = currentMillis;
		flag = 0;                               // Готов к расшифровке пакета. Повторная проверка готовности пакета не требуется
		WeaterI2C2.writeByte(0x01, 14, flag);   // Отправляем состояние флага ведомому (адрес ведомого 0x01, номер регистра 14, состояние флага)
		byte var = 0;                           // Переменная попыток получить информацию с приемника Misol

		while (var < 5)                         // Пять попыток получить информацию с приемника Misol
		{
			for (int i = 0; i < 14; i++)
			{
				REG_Array1[i] = WeaterI2C2.readByte(0x01, i);        // Считываем пакет из приемника метеостанции
				delay(10);

			}

			#ifdef MISOL_DEBUG
	
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.println("Packet");
				for (int i = 0; i < 14; i++)
				{
					 Serial4.print(REG_Array1[i], HEX);
					 Serial4.print(" / ");
				}
				 Serial4.println();
			}
			#endif

			crc = REG_Array1[12];                              // Считываем принятую контрольную сумму из пакета
			if (crc == calc_REG_Array())                       // Если данные верны - завершить попытки чтения данных
			{
				#ifdef MISOL_DEBUG
	 			if (debug.MISOL_DEBUG_K == HIGH)
				{
					 Serial4.print("count receive - ");
					 Serial4.println(var);

					 Serial4.print("crc1 - ");
					 Serial4.println(crc);
					 Serial4.print("calc_REG_Array - ");
					 Serial4.println(calc_REG_Array());
				}
				#endif
					stationID = REG_Array1[0];              // Считываем ID станции
				break;
			}
			var++;
			delay(50);
		}


		if (stationID == _ID_Misol_WS0232 && crc == calc_REG_Array())  // Принят пакет от станции MISOL WS0232
		{ //первый байт должен быть 0xF5 (ID станции, может варьироваться от модели и верный CRC
			//ID станции возможно необходимо настраивать под конкретную?
			#ifdef MISOL_DEBUG

			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.print("Misol WS0232 ID -");
				 Serial4.println(stationID);
			}
			#endif

			bool tempValid = true;
			byte tempI2C_temp1 = REG_Array1[4];         // Считываем старший байт значения температуры ведомого (адрес ведомого 0x01, номер регистра 1)
			byte tempI2C_temp2 = tempI2C_temp1 & 0x0F;  // удалить старшие биты
			tempI2C = tempI2C_temp2 << 8;               // сдвигаем полученный байт на 8 бит влево, т.к.он старший
			tempI2C += REG_Array1[5];                   // Младший байт значения температуры

			#ifdef MISOL_DEBUG
				if (debug.MISOL_DEBUG_K == HIGH)
				{
					 Serial4.print("tempI2C -");
					 Serial4.println(tempI2C);
				}
			#endif

			if (tempI2C != 255)
			{
				if (tempI2C > 2048)                    // Проверяем на минусовые температуры
				{
					tempI2C = tempI2C - 2048;
					tempI2C = -tempI2C;
				}

				temp = (float)(tempI2C) / 10.0;
			}
			else
			{
				tempValid = false;
			}

			if (tempValid) // валидная температура
			{
				int32_t iTemp = temp * 100;
				Temperature = iTemp / 100;
				TemperatureDecimal = abs(iTemp % 100);

			}
			else // невалидная температура
			{
				Temperature = NO_TEMPERATURE_DATA;
				TemperatureDecimal = 0;
			}
			WeatherStation.TemperatureIsOK = tempValid;

			//Влажность
			bool humValid = true;
	   		humI2C = REG_Array1[6];  // Влажность воздуха

			if (humI2C != 0xFF) // валидная влажность
			{
				Humidity = humI2C;
				HumidityDecimal = 0;
			}
			else // невалидная влажность
			{
				Humidity = NO_TEMPERATURE_DATA;
				HumidityDecimal = 0;
				humValid = false;
			}
			WeatherStation.HumidityIsOK = humValid;

			//скорость ветра
			uint16_t ch_wind = 0;
			uint16_t ch_wind1 = 0;
			float wind = 0;
			bool windValid = true;

			windI2C = REG_Array1[7] << 8;  // Старший байт скорости ветра
			windI2C += REG_Array1[8];      // Младший байт скорости ветра

			if (windI2C != 0xFF)
			{
				wind = (float)windI2C / 588;
			}
			else
			{
				windValid = false;
			}

			if (windValid) // если данные по скорости ветра валидны
			{
				// метеостанция передаёт нам скорость ветра в метрах в секунду
				// а у нас скорость ветра хранится в сотых долях м/с
				// поэтому - конвертируем значение в сотые доли
				uint32_t windSpeed = wind * 100;

				// просим модуль логики сохранить данные по скорости ветра
				LogicManageModule->SetWindSpeed(windSpeed);

			}// if(windValid)

			//=================направление ветра==============================

			byte wind_dirI2C_temp1 = REG_Array1[4];
			byte wind_dirI2C_temp2 = wind_dirI2C_temp1 >> 4;
			wind_dirI2C = wind_dirI2C_temp2 & 0x0F;

			if (wind_dirI2C != 0xFF) // если данные по направлению ветра валидны
			{
				CompassPoints windDirection = cpUnknown;
				switch (wind_dirI2C)
				{
				case 0: /* Serial4.print("N")*/ windDirection = cpNorth; break;
				case 2: /* Serial4.print("NE")*/ windDirection = cpNorth; break;
				case 4: /* Serial4.print("E")*/ windDirection = cpEast; break;
				case 6: /* Serial4.print("SE")*/ windDirection = cpEast; break;
				case 8: /* Serial4.print("S")*/ windDirection = cpSouth; break;
				case 10: /* Serial4.print("SW")*/ windDirection = cpSouth; break;
				case 12: /* Serial4.print("W")*/ windDirection = cpWest; break;
				case 14: /* Serial4.print("NW")*/ windDirection = cpWest; break;
				default: windDirection = cpUnknown; break;
				} // switch

				// просим модуль логики сохранить данные по направлению ветра
				LogicManageModule->SetWindDirection(windDirection);

			} // if(wind_dir != 0xFF)

				//========== Дождь================================= 

	
			uint8_t rain = REG_Array1[11];  //Передает состояние счетчика импульсов коромысла. Для определения наличия дождя нужно сравнивать предыдущие данные и текущие.


			if (rain != 0xFF) // если данные по дождю валидны
			{
				bool hasRain = false;
				if (lastRainPulse == 0xFF) // ещё не сохраняли последнее значение с метеостанции
				{
					lastRainPulse = rain; // сохраняем его
				}

				if (rain != lastRainPulse) // если значения не равны - идёт дождь
				{
					lastRainPulse = rain;
					hasRain = true;
				}
				// просим модуль логики сохранить флаг дождя
				LogicManageModule->SetHasRain(hasRain);
			} // if(rain != 0xFF)

            #ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.print("\n");

				 Serial4.print("Temperature: ");
				 Serial4.print((temp), 1);
				 Serial4.println("C");
				 Serial4.print("Humidity: ");
				 Serial4.print(Humidity);
				 Serial4.println("%");
				 Serial4.print("Wind: ");
				 Serial4.print(wind);
				 Serial4.println("m/s");
				 

				switch (wind_dirI2C)
				{
					Serial4.print("Direction: ");
				case 0:  Serial4.print("N"); break;
				case 2:  Serial4.print("NE"); break;
				case 4:  Serial4.print("E"); break;
				case 6:  Serial4.print("SE"); break;
				case 8:  Serial4.print("S"); break;
				case 10:  Serial4.print("SW"); break;
				case 12:  Serial4.print("W"); break;
				case 14:  Serial4.print("NW"); break;
				default:  Serial4.print("Error"); break;
				}

				 Serial4.print("\n");

				 Serial4.print("Rain pulse: ");  // счетчик дождя
				 Serial4.println(rain);

				 Serial4.print("\n\n");
			}
            #endif

			for (int i = 0; i < 15; i++)
			{
				REG_Array1[i] = 0;        // Очистить пакет из приемника метеостанции
			}

			WeatherStation.WeatherIsOK = true;
		}
		//else
		//{
		//	WeatherStation.WeatherIsOK = false;
		//}



		if (stationID == _ID_Misol_WN5300CA && crc == calc_REG_Array())   // Принят пакет от станции MISOL WN5300CA
		{ //первый байт должен быть 0x6D (ID станции, может варьироваться от модели и верный CRC
			//ID станции возможно необходимо настраивать под конкретную?
			#ifdef MISOL_DEBUG
			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.print("Misol WN5300CA ID -");
				 Serial4.println(stationID);
			}
			#endif
			
			bool tempValid = true;

			TempBitError = REG_Array1[1];       // ошибки показаний температуры
			tempI2C = REG_Array1[2]<<8;         // Считываем старший байт значения температуры ведомого (адрес ведомого 0x01, номер регистра 1)
			tempI2C += REG_Array1[3];           // Младший байт значения температуры

			// Функция получения температуры в гр. С
	
			if (TempBitError == false)
			{
				temp = ((float)(tempI2C)-400)/10.0;
			}
			else
			{
				tempValid = false;
			}


			/* Проверить на отрицательные температуры*/
			if (tempI2C != 255)
			{
				if (tempI2C > 2048)                    // Проверяем на минусовые температуры
				{
					tempI2C = tempI2C - 2048;
					tempI2C = -tempI2C;
				}

				temp = (float)(tempI2C) / 10.0;
			}
			else
			{
				tempValid = false;
			}


			if (tempValid) // валидная температура
			{
				int32_t iTemp = temp * 100;
				Temperature = iTemp / 100;
				TemperatureDecimal = abs(iTemp % 100);

			}
			else // невалидная температура
			{
				Temperature = NO_TEMPERATURE_DATA;
				TemperatureDecimal = 0;
			}
			WeatherStation.TemperatureIsOK = tempValid;
			//Влажность
	
			humI2C = REG_Array1[4];  // Влажность воздуха

			if (humI2C != 0xFF)       // валидная влажность
			{
				Humidity = humI2C;
				HumidityDecimal = 0;
			}
			else // невалидная влажность
			{
				Humidity = NO_TEMPERATURE_DATA;
				HumidityDecimal = 0;
			}

			WeatherStation.TemperatureIsOK = tempValid;
			//скорость ветра

			float wind = 0;
			bool windValid = true;

			gust_wind = REG_Array1[6] ;     //  байт порыва ветра
			windI2C   = REG_Array1[5];      //  байт скорости ветра

			if (windI2C != 0xFF)
			{
				wind = (float)windI2C*0.34;
			}
			else
			{
				windValid = false;
			}

			if (windValid) // если данные по скорости ветра валидны
			{
				// метеостанция передаёт нам скорость ветра в километрах в час
				// а у нас скорость ветра хранится в сотых долях м/с
				// поэтому - конвертируем значение в сотые доли
				uint32_t windSpeed = wind * 100;

				// просим модуль логики сохранить данные по скорости ветра
				LogicManageModule->SetWindSpeed(windSpeed);

			}// if(windValid)

			//=================направление ветра==============================

			wind_dirI2C = REG_Array1[11];

			if (wind_dirI2C != 0xFF) // если данные по направлению ветра валидны
			{
				CompassPoints windDirection = cpUnknown;
				switch (wind_dirI2C)
				{
				case 0: /* Serial4.print("N")*/   windDirection = cpNorth; break;
				case 1: /* Serial4.print("NNE")*/ windDirection = cpNorth; break;
				case 2: /* Serial4.print("NE")*/  windDirection = cpNorth; break;
				case 3: /* Serial4.print("NEE")*/ windDirection = cpNorth; break;

				case 4: /* Serial4.print("E")*/   windDirection = cpEast; break;
				case 5: /* Serial4.print("SEE")*/ windDirection = cpEast; break;
				case 6: /* Serial4.print("ES")*/  windDirection = cpEast; break;
				case 7: /* Serial4.print("SSE")*/ windDirection = cpEast; break;

				case 8: /* Serial4.print("S")*/   windDirection = cpSouth; break;
				case 9: /* Serial4.print("SSW")*/ windDirection = cpSouth; break;
				case 10: /* Serial4.print("SW")*/ windDirection = cpSouth; break;
				case 11: /* Serial4.print("SWW")*/ windDirection = cpSouth; break;

				case 12: /* Serial4.print("W")*/   windDirection = cpWest; break;
				case 13: /* Serial4.print("NWW")*/ windDirection = cpWest; break;
				case 14: /* Serial4.print("WN")*/  windDirection = cpWest; break;
				case 15: /* Serial4.print("NWN")*/ windDirection = cpWest; break;

				default: windDirection = cpUnknown; break;
				} // switch

				// просим модуль логики сохранить данные по направлению ветра
				LogicManageModule->SetWindDirection(windDirection);

			} // if(wind_dir != 0xFF)

				//========== Дождь================================= 

				uint16_t rain = REG_Array1[7] << 8;  // Старший байт. Передает состояние счетчика импульсов коромысла. Для определения наличия дождя нужно сравнивать предыдущие данные и текущие.
			             rain = REG_Array1[8];       // Младший байт. Передает состояние счетчика импульсов коромысла. Для определения наличия дождя нужно сравнивать предыдущие данные и текущие.
					     rain = rain * 0.3; // 

			if (rain != 0xFF) // если данные по дождю валидны
			{
				bool hasRain = false;
				if (lastRainPulse == 0xFF) // ещё не сохраняли последнее значение с метеостанции
				{
					lastRainPulse = rain; // сохраняем его
				}

				if (rain != lastRainPulse) // если значения не равны - идёт дождь
				{
					lastRainPulse = rain;
					hasRain = true;
				}
				// просим модуль логики сохранить флаг дождя
				LogicManageModule->SetHasRain(hasRain);
			} // if(rain != 0xFF)

            #ifdef MISOL_DEBUG

			if (debug.MISOL_DEBUG_K == HIGH)
			{
				 Serial4.print("\n");

				 Serial4.print("Temperature: ");
				 Serial4.print((temp), 1);
				 Serial4.println("C");
				 Serial4.print("Humidity: ");
				 Serial4.print(Humidity);
				 Serial4.println("%");
				 Serial4.print("Wind: ");
				 Serial4.print(wind);
				 Serial4.println("m/s");
				 Serial4.print("Direction: ");

				switch (wind_dirI2C)
				{
				case 0:  Serial4.print("N"); break;
				case 2:  Serial4.print("NE"); break;
				case 4:  Serial4.print("E"); break;
				case 6:  Serial4.print("SE"); break;
				case 8:  Serial4.print("S"); break;
				case 10:  Serial4.print("SW"); break;
				case 12:  Serial4.print("W"); break;
				case 14:  Serial4.print("NW"); break;
				default:  Serial4.print("Error"); break;
				}

				 Serial4.print("\n");

				 Serial4.print("Rain pulse: ");  // счетчик дождя
				 Serial4.println(rain);

				 Serial4.print("\n\n");
			}
            #endif

			for (int i = 0; i < 15; i++)
			{
				REG_Array1[i] = 0;        // Очистить пакет из приемника метеостанции
			}

			WeatherStation.WeatherIsOK = true;
		}
		//else
		//{
		//   WeatherStation.WeatherIsOK = false;
		//}
  	}
}


int WeatherStationClass::calc_REG_Array()
{
	uint8_t tmp_byte = 0;
	uint16_t tmp_result = 0;
		for (uint8_t j = 0; j < 12; j++)
		{
			tmp_byte = 0;
			tmp_byte = REG_Array1[j];
			tmp_result = tmp_result + tmp_byte;
		}
	return (tmp_result & 0xFF);
}

double WeatherStationClass::getTemperature()
{
	if (TempBitError == 0)
	{
	
		return ((double)tempI2C - 400) / 10.0;
	}
}

void WeatherStationClass::SetConnectReceiver(bool b)
{
	connectReceiver = b;                           // Флаг подключения к приемнику станции по шине I2C
}
bool WeatherStationClass::GetConnectReceiver()
{
	return connectReceiver;                       // Флаг подключения к приемнику станции по шине I2C
}

void WeatherStationClass::SetDataReceive(bool b)
{
	dataReceive = b;                           // Флаг наличия новых данных
}
bool WeatherStationClass::GetDataReceive()
{
	return dataReceive;                       // Флаг наличия новых данных
}

void WeatherStationClass::SetDataReceived(bool b)
{
	dataReceived = b;                           // Флаг наличия новых данных
}
bool WeatherStationClass::GetDataReceived()
{
	return dataReceived;                       // Флаг наличия новых данных
}

//--------------------------------------------------------------------------------------------------------------------------------------

