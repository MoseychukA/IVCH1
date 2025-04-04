#ifndef _UNI_GLOBALS_H
#define _UNI_GLOBALS_H
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#define UNUSED(expr) do { (void)(expr); } while (0)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
//Структура передаваемая мастеру и обратно
//-------------------------------------------------------------------------------------------------------------------------------------------------------
enum {RS485FromMaster = 1, RS485FromSlave = 2};
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  uint32_t WindowsState;             // состояние каналов окон, 4 байта = 32 бита = 16 окон)
  uint16_t WaterChannelsState;       // состояние каналов полива, 2 байта, (16 каналов)
  byte LightChannelsState;           // состояние каналов досветки, 1 байт (8 каналов)
  byte PinsState[16];                // состояние пинов, 16 байт, 128 пинов
} ControllerState;                   // состояние контроллера
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  byte header1;
  byte header2;

  byte direction;                     // направление: 1 - от меги, 2 - от слейва
  byte type;                          // тип: 1 - пакет исполнительного модуля, 2 - пакет модуля с датчиками

  byte data[sizeof(ControllerState)]; // N байт данных, для исполнительного модуля в этих данных содержится состояние контроллера
  // для модуля с датчиками: первый байт - тип датчика, 2 байт - его индекс в системе. В обратку модуль с датчиками должен заполнить показания (4 байта следом за индексом 
  // датчика в системе и отправить пакет назад, выставив direction и type.

  byte tail1;
  byte tail2;
  byte crc8;
  
} RS485Packet;                     // пакет, гоняющийся по RS-485 туда/сюда (30 байт)
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  msNormal,                        // нормальное состояние конечного автомата
  msWaitForTankFullfilled,         // ждём наполнения бака
  msFillTankError,                 // бак не заполнился в течение нужного времени
  
} MachineState;
//-------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  waterTankNoErrors            = 0, // нет ошибок
  waterTankNoData              = 1, // нет внешних данных в течение долгого времени
  waterTankFullSensorError     = 2, // не сработал датчик верхнего уровня в процессе наполнения, по превышению максимального времени наполнения
  waterTankTopSensorFailure    = 3, // ошибка нижнего датчика критического уровня
  
} WaterTankErrorType;
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  uint8_t valveState;              // статус клапана наполнения бочки
  uint8_t fillStatus;              // статус наполнения (0-100%)
  uint8_t errorFlag;               // флаг наличия ошибки
  uint8_t errorType;               // тип ошибки
  uint8_t reserved[19];            // добитие до 23 байт

} WaterTankDataPacket;
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  uint8_t valveCommand;           // флаг - включить клапан бака воды или выключить
  uint8_t reserved[22];           // добитие до 23 байт

} RS485WaterTankCommandPacket;
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  uint8_t level;                  // флаг - включить клапан бака воды или выключить
  uint32_t maxWorkTime;
  uint32_t distanceEmpty;
  uint32_t distanceFull;
  uint8_t reserved[14];           // добитие до 23 байт

} RS485WaterTankSettingsPacket;
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  byte packet_type;               // тип пакета
  byte packet_subtype;            // подтип пакета
  byte config;                    // конфигурация
  byte controller_id;             // ID контроллера, к которому привязан модуль
  byte rf_id;                     // идентификатор RF-канала модуля
  
} UniScratchpadHead; // голова скратчпада, общая для всех типов модулей
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  UniScratchpadHead head;         // голова
  byte data[24];                  // сырые данные
  byte crc8;                      // контрольная сумма
  
} UniRawScratchpad;               // "сырой" скратчпад, байты данных могут меняться в зависимости от типа модуля
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
typedef enum
{
  uniSensorsClient       = 1,     // packet_type == 1
  uniNextionClient       = 2,     // packet_type == 2
  uniExecutionClient     = 3,     // packet_type == 3
  uniWindRainClient      = 4,     // packet_type == 4
  uniSunControllerClient = 5,     // packet_type == 5
  uniWaterTankClient     = 6,     // packet_type == 6
  
} UniClientType; // тип клиента
//-------------------------------------------------------------------------------------------------------------------------------------------------------
enum 
{
  RS485ControllerStatePacket      = 1, 
  RS485SensorDataPacket           = 2, 
  RS485WindowsPositionPacket      = 3,
  RS485RequestCommandsPacket      = 4,
  RS485CommandsToExecuteReceipt   = 5,
  RS485SensorDataForRemoteDisplay = 6,
  RS485SettingsForRemoteDisplay   = 7,
  RS485WindRainData               = 8,  // запрос данных по дождю, скорости, направлению ветра
  RS485SunControllerData          = 9,  // пакет с данными контроллера солнечной установки
  RS485WaterTankCommands         = 10,  // пакет с командами для модуля контроля бака воды
  RS485WaterTankSettings         = 11,  // пакет с настройками для модуля контроля уровня бака
  RS485WaterTankRequestData      = 12,  // пакет с запросом данных по баку с водой
  RS485WaterTankDataAnswer       = 13,  // пакет с ответом на запрос данных по баку с водой
  
};
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  byte controller_id;                  // ID контроллера, который выплюнул в эфир пакет
  byte packetType;                     // тип пакета
  byte valveCommand;                   // флаг - включить клапан бака воды или выключить
  byte reserved[26];                   // резерв, добитие до 30 байт
  byte crc8;                           // контрольная сумма
  
} NRFWaterTankExecutionPacket;         // пакет с командами для модуля контроля бака воды
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct
{
  byte controller_id;                 // ID контроллера, который выплюнул в эфир пакет
  byte packetType;                    // тип пакета
  byte level;                         // уровень срабатывания датчиков
  uint32_t maxWorkTime;               // максимальное время работы, секунд
  uint16_t distanceEmpty;             // расстояние до пустого бака, см
  uint16_t distanceFull;              // расстояние до полного бака, см              
  byte reserved[18];                  // резерв, добитие до 30 байт
  byte crc8;                          // контрольная сумма
  
} NRFWaterTankSettingsPacket;         // пакет с настройками для модуля контроля бака воды
#pragma pack(pop)
//-------------------------------------------------------------------------------------------------------------------------------------------------------
#endif
