#pragma once

#include <Arduino.h>

//--------------------------------------------------------------------------------------------------------------------------------------
class WeatherStationClass
{
  private:
      int16_t _ID_Misol;
	  int16_t _ID_Misol_WS0232 = 255;
	  int16_t _ID_Misol_WN5300CA = 255;
 	  int calc_REG_Array();
	  double getTemperature();
	  byte TempBitError;                       //  Бит(-ы, почему-то 2) ошибки показаний температуры
	  unsigned long currentMillis = 0;
	  bool connectI2C_Ok  = false;             // Флаг подключения к приемнику станции по шине I2C
	  bool connectReceiver = false;            // Флаг подключения к приемнику станции по шине I2C
	  bool connectMisol   = false;             // Флаг приема данных с метеостанции Misol
	  bool dataReceive = false;                // Флаг наличия новых данных
	  bool dataReceived = false;               // Флаг наличия новых данных

	  int8_t connectReceiverMisol = 10;
	  int8_t I2C_Ok;                           // состояние флага наличия подключения приемника метеостанции
	  int8_t count_var = 0;

  public:
    WeatherStationClass();

    void setup_WS0232(int16_t _ID_Misol);
	void setup_WN5300CA(int16_t _ID_Misol);
    void update();

	void SetConnectReceiver(bool b);
	bool GetConnectReceiver();
	void SetDataReceive(bool b);
	bool GetDataReceive();
	void SetDataReceived(bool b);
	bool GetDataReceived();


  int8_t Humidity;            // целое значение влажности
  uint8_t HumidityDecimal;    // значение влажности после запятой
  int8_t Temperature;         // целое значение температуры
  uint8_t TemperatureDecimal; // значение температуры после запятой
  bool WeatherIsOK;           // Данные получены
  bool HumidityIsOK;          // Данные получены
  bool TemperatureIsOK;       // Данные получены
};
//--------------------------------------------------------------------------------------------------------------------------------------
extern WeatherStationClass WeatherStation;
