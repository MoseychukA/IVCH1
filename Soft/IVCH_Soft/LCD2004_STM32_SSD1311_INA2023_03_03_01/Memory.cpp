#include "Memory.h"
#include "AT24CX.h"
#include "Configuration_STM32.h"  // Основные настройки программы
#include "LCDMenu.h"
#include "Settings.h"
#include "CoreCommandBuffer.h"

AT24CX* mem;

//--------------------------------------------------------------------------------------------------------------------------------
void MemInit()
{

#if EEPROM_USED_MEMORY == EEPROM_AT24C32
    mem = new AT24C32(EEPROM_MEMORY_INDEX);
#elif EEPROM_USED_MEMORY == EEPROM_AT24C64
    mem = new AT24C64(EEPROM_MEMORY_INDEX);
#elif EEPROM_USED_MEMORY == EEPROM_AT24C128
    mem = new AT24C128(EEPROM_MEMORY_INDEX);
#elif EEPROM_USED_MEMORY == EEPROM_AT24C256
    mem = new AT24C256(EEPROM_MEMORY_INDEX);
#elif EEPROM_USED_MEMORY == EEPROM_AT24C512
    mem = new AT24C512(EEPROM_MEMORY_INDEX);
#endif  
}

//--------------------------------------------------------------------------------------------------------------------------------
void MemClear()
{
    LCD_Class* dc = LCDScreen->getDC();
 
    if (!dc)
    {
        return;
    }


    uint32_t len = 0;

    len = 16384; // Settings.GetMemorySize();

    Settings.displayBacklight(true);                                 // Включить подсветку дисплея

    dc->clear();
	DBG("Max address - ");
	DBGLN(len);

    int step_clear = len / 20;
    byte step_cursor = 0;
    char str_clear[1] = {0xFE};
    int step_reset = 2048;                                       // Интералы сброса таймера ничего не деланья

    char str[20];
    itoa(len, str, 10);                                           // Записать в строку номер сообщения в памяти
    dc->clear();                                                  // Стереть экран
    dc->setCursor(0, 0);                                          // Установить курсор в начало экрана
    dc->print("Start setting clear.");
    dc->setCursor(0, 1);
    dc->print("Size - ");
    dc->print(str);

    dc->powerMode(SSD1311_LCD_OFF);
    dc->cursor_on = false;                       // 
    dc->cursor_blinking = true;                      // 
    dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
    dc->BDC = false;
    dc->setEntryMode();
    dc->powerMode(SSD1311_LCD_ON);

    uint32_t address = 0;
    DBGLN(F("Start setting clear."));
    for (address = 0; address < len; address++)
    {
        mem->write(address, 0x00);
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
            DBGLN(str);
         }
        //step_reset таймер
        if ((address % step_reset) == 0) // периодически сбрасываем таймер ничего не деланья
        {
            LCDScreen->resetIdleTimer();

        }

    } // for

  

	//DBGLN("End..");

    dc->powerMode(SSD1311_LCD_OFF);
    dc->cursor_on = false;                      // 
    dc->cursor_blinking = false;                      // 
    dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
    dc->BDC = false;
    dc->setEntryMode();
    dc->powerMode(SSD1311_LCD_ON);
    MemWrite(0, MEM_CONTROL_BYTE);                              // Запись контрольного бита, означающего инициализацию памяти
    MemWrite(All_Count_Text_Message_ADDRESS, 0);                // адрес хранения счетчика общего количества записей
    MemWrite(Count_NotRead_Message_ADDRESS, 0);                 // адрес хранения счетчика не прочитанного количества записей
    MemWrite(Current_Counter_Message, 0);                       // адрес хранения текущего счетчика количества записей
    MemWrite(Flipping_Counter_Message, 0);                      // адрес хранения счетчика листания записей
   // MemWrite(TmpCount_Flip_Message_ADDRESS, 0);                 // адрес хранения временного счетчика количества записей
    MemWrite(Response_message_block_ADDRESS, 0);                // начальный адрес хранения счетчика ответных сообщений
    MemWrite(Current_Counter_confirmation, 0);                  // сохраним новый номер сохраненного подтверждения.
    dc->setCursor(0, 3);                                        // Установить курсор в начало экрана
    dc->print("End..");                                       // Отобразить новое сообщение
    DBGLN(F("EEPROM clearance END"));
    delay(1000);
    CommandHandler.Stm32_SoftReset();

}

//void ClearMessage()
//{
//    LCD_Class* dc = LCDScreen->getDC();
//
//    if (!dc)
//    {
//        return;
//    }
//
//    uint32_t len = 0;
//
//    // Определяем объем установленной памяти
//   // LCDScreen->resetIdleTimer();
//    len = Settings.getCurrentCountMessage();                     // Получить количество записанных сообщений
//    len = (len * Number_of_bytes_block) + Response_message_block_ADDRESS;    // Расчитать конечный адрес сообщений 
//
//    Settings.displayBacklight(true);                             // Включить подсветку дисплея
//
//    int step_clear = len / 20;                                   // Шаг перемещения индикации стирания на дисплее
//    int step_reset = 2048;                                       // Интералы сброса таймера ничего не деланья
//    byte step_cursor = 0;                                        // 
//
//    char str[20];
//    itoa(len, str, 10);                                          // Записать в строку номер сообщения в памяти
//    dc->clear();                                                 // Стереть экран
//    dc->setCursor(0, 0);                                         // Установить курсор в начало экрана
//    dc->print("Start MESSAGE clear..");
//    dc->setCursor(0, 1);
//    dc->print("Size - ");
//    dc->print(str);
//
//    /* включить мигание курсора */
//    dc->powerMode(SSD1311_LCD_OFF);                            // отключить дисплей на время настройки мигания знакоместа
//    dc->cursor_on = false;                                     // подчеркивание курсора отключить
//    dc->cursor_blinking = true;                                // оставить только мигание знакоместа
//    dc->cursor_direction = SSD1311_DIRECTION_RIGHT;            // перемещение курсора вправо
//    dc->BDC = false;                                           // не знаю, но применяю :-)
//    dc->setEntryMode();                                        // ввести настройки в дисплей
//    dc->powerMode(SSD1311_LCD_ON);                             // включить дисплей
//
//
//    for (uint32_t address = Response_message_block_ADDRESS; address < len; address++)
//    {
//        MemWrite(address, 0x00);
//        #ifdef USE_WATCHDOG_TIMER
//        Settings.reset_IWDG();      // Сброс сторожевого таймера 
//        #endif
//        if ((address % step_clear) == 0)
//        {
//            itoa(address, str, 10);                              // Записать в строку номер текущего адреса стирания
//            dc->setCursor(14, 1);                                // Установить курсор в начало экрана
//            dc->print(str);                                      // отобразить номер текущего адреса стирания
//            dc->setCursor(step_cursor, 2);                       // установить курсор
//            step_cursor++;
//            dc->setCursor(step_cursor - 1, 2);
//            dc->print("X");
//            dc->setCursor(step_cursor - 1, 2);
//        }
//        //step_reset таймер
//        if ((address % step_reset) == 0)                         // периодически сбрасываем таймер ничего не деланья
//        {
//            LCDScreen->resetIdleTimer();
//        }
//    } // for
//
//    delay(1000);
//
//    uint32_t address = Start_confirmation_ADDRESS;
//    byte count_confirmation = MemRead(Current_Counter_confirmation);   // Получить номер текущего подтверждения
//    itoa(address, str, 10);                                          // Записать в строку номер сообщения в памяти
//    dc->setCursor(0, 1);
//    dc->print("Size - ");
//    dc->print(str);
//    dc->setCursor(12, 1);                                // Установить курсор в начало экрана
//    dc->print("        ");
//
//    for (address; address < (Number_of_bytes_confirmation * count_confirmation)+ Start_confirmation_ADDRESS; address++)
//    {
//        mem->write(address, 0x00);
//        #ifdef USE_WATCHDOG_TIMER 
//        Settings.reset_IWDG();      // Сброс сторожевого таймера 
//        #endif
//        if ((address % step_clear) == 0)
//        {
//            itoa(address, str, 10);                              // Записать в строку номер сообщения в памяти
//            dc->setCursor(14, 1);                                // Установить курсор в начало экрана
//            dc->print(str);
//            dc->setCursor(step_cursor, 2);
//            step_cursor++;
//            dc->setCursor(step_cursor - 1, 2);
//            dc->print("X");
//            dc->setCursor(step_cursor - 1, 2);
//        }
//    } // for
//
//    delay(1000);
//  //  DBGLN("End..");
//
//    //LCDScreen->resetIdleTimer();
//    dc->powerMode(SSD1311_LCD_OFF);
//    dc->cursor_on = false;                      // 
//    dc->cursor_blinking = false;                      // 
//    dc->cursor_direction = SSD1311_DIRECTION_RIGHT;
//    dc->BDC = false;
//    dc->setEntryMode();
//    dc->powerMode(SSD1311_LCD_ON);
//    MemWrite(All_Count_Text_Message_ADDRESS, 0);                // адрес хранения счетчика общего количества записей
//    MemWrite(Count_NotRead_Message_ADDRESS, 0);                 // адрес хранения счетчика не прочитанного количества записей
//    MemWrite(Current_Counter_Message, 0);                       // адрес хранения текущего счетчика количества записей
//    MemWrite(Flipping_Counter_Message, 0);                      // адрес хранения счетчика листания записей
//  //  MemWrite(TmpCount_Flip_Message_ADDRESS, 0);               // адрес хранения временного счетчика количества записей
//    MemWrite(Response_message_block_ADDRESS, 0);                // начальный адрес хранения счетчика ответных сообщений
//    MemWrite(Current_Counter_confirmation, 0);                  // сохраним новый номер сохраненного подтверждения.
//
//    dc->setCursor(0, 3);                                        // Установить курсор в начало экрана
//  //  dc->printRus("ЗАВЕРШЕНО ", 0, 3);
//    dc->print("End..");                                         // Отобразить новое сообщение
//    //DBGLN(F("EEPROM clearance END"));
//   // LCDScreen->switchToScreen("MAIN");
//   // CommandHandler.Stm32_SoftReset();
//   // Stm32_SoftReset();
//}


//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteChars(unsigned int address, char* data, int length)
{
    mem->writeChars(address,data, length);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemReadChars(unsigned int address, char* data, int n)
{
    mem->readChars(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte data)
{
    mem->write(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWrite(unsigned int address, byte* data, int n)
{
    mem->write(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteInt(unsigned int address, unsigned int data)
{
    mem->writeInt(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteLong(unsigned int address, unsigned long data)
{
    mem->writeLong(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteFloat(unsigned int address, float data)
{
    mem->writeFloat(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------
void MemWriteDouble(unsigned int address, double data)
{
    mem->writeDouble(address, data);
}
//--------------------------------------------------------------------------------------------------------------------------------



//--------------------------------------------------------------------------------------------------------------------------------
void MemRead(unsigned int address, byte* data, int n)
{
    mem->read(address, data, n);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned int MemReadInt(unsigned int address)
{
    mem->readInt(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
unsigned long MemReadLong(unsigned int address)
{
    mem->readLong(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
float MemReadFloat(unsigned int address)
{
    mem->readFloat(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
double MemReadDouble(unsigned int address)
{
    mem->readDouble(address);
}
//--------------------------------------------------------------------------------------------------------------------------------
uint8_t MemRead(unsigned int address)
{
    return mem->read(address);
}
//--------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------------------------------------


/*    Примеры применения  */
/*
  // read and write byte
  Serial.println("Write 42 to address 12");
  mem.write(12, 42);
  Serial.println("Read byte from address 12 ...");
  byte b = mem.read(12);
  Serial.print("... read: ");
  Serial.println(b, DEC);
  Serial.println();
  
  // read and write integer
  Serial.println("Write 65000 to address 15");
  mem.writeInt(15, 65000);
  Serial.println("Read integer from address 15 ...");
  unsigned int i = mem.readInt(15);
  Serial.print("... read: ");
  Serial.println(i, DEC);
  Serial.println();

  // read and write long
  Serial.println("Write 3293732729 to address 20");
  mem.writeLong(20, 3293732729UL);
  Serial.println("Read long from address 20 ...");
  unsigned long l = mem.readLong(20);
  Serial.print("... read: ");
  Serial.println(l, DEC);
  Serial.println();

  // read and write long
  Serial.println("Write 1111111111 to address 31");
  mem.writeLong(31, 1111111111);
  Serial.println("Read long from address 31 ...");
  unsigned long l2 = mem.readLong(31);
  Serial.print("... read: ");
  Serial.println(l2, DEC);
  Serial.println();
  
  // read and write float
  Serial.println("Write 3.14 to address 40");
  mem.writeFloat(40, 3.14);
  Serial.println("Read float from address 40 ...");
  float f = mem.readFloat(40);
  Serial.print("... read: ");
  Serial.println(f, DEC);
  Serial.println();  

  // read and write double
  Serial.println("Write 3.14159265359 to address 50");
  mem.writeDouble(50, 3.14159265359);
  Serial.println("Read double from address 50 ...");
  double d = mem.readDouble(50);
  Serial.print("... read: ");
  Serial.println(d, DEC);
  Serial.println();
  
  // read and write char
  Serial.print("Write chars: '");
  char msg[] = "This is a message";
  Serial.print(msg);
  Serial.println("' to address 200");
  mem.writeChars(200, msg, sizeof(msg));
  Serial.println("Read chars from address 200 ...");
  char msg2[30];
  mem.readChars(200, msg2, sizeof(msg2));
  Serial.print("... read: '");
  Serial.print(msg2);
  Serial.println("'");
  Serial.println();

  // write array of bytes 
  Serial.println("Write array of 80 bytes at address 1000");
  byte xy[] = {0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,8,8,8,9,9,9,    // 10 x 3 = 30
              10,11,12,13,14,15,16,17,18,19,                                   //          10
              120,121,122,123,124,125,126,127,128,129,                         //          10
              130,131,132,133,134,135,136,137,138,139,                         //          10
              200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219};                   //          20
  mem.write(1000, (byte*)xy, sizeof(xy));

  // read bytes with multiple steps
  Serial.println("Read 80 single bytes starting at address 1000");
  for (int i=0; i<sizeof(xy); i++) {
    byte sb = mem.read(1000+i);
    Serial.print("[");
    Serial.print(1000+i);
    Serial.print("] = ");
    Serial.println(sb);
  } 
  Serial.println();

  // read bytes with one step
  Serial.println("Read 80 bytes with one operation at address 1000");
  byte z[80];
  memset(&z[0], 32, sizeof(z));
  mem.read(1000, z, sizeof(z));
  for (int i=0; i<sizeof(z); i++) {
    Serial.print("[");
    Serial.print(1000+i);
    Serial.print("] = ");
    Serial.println(z[i]);
  } 
  
*/