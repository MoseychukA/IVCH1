//**************************************************************************************************
// Filename                     main.c
// Platform                     STM32F10x
// Author                       Aveal (http://microtechnics.ru/)
// Current version              1.0.0   
// Date                         11.01.2015
//**************************************************************************************************



//**************************************************************************************************
// Includes
//**************************************************************************************************

#include "main.h"



//**************************************************************************************************
// Declarations and definitions
//**************************************************************************************************

SD_CardInfo SDCard;
typedef  void (*pFunction)(void);
uint8_t readBuffer[512];
uint32_t programBytesToRead;
uint32_t programBytesCounter;
uint32_t currentAddress;
UINT readBytes;
uint32_t jumpAddress;
pFunction Jump_To_Application;
FRESULT result;


//**************************************************************************************************
// Global functions
//**************************************************************************************************



//**************************************************************************************************
// Function      SetKey()
// Description   Sets bootloader key
// Parameters    None
// RetVal        None
//**************************************************************************************************
void SetKey()
{
  FLASH_Unlock();
  FLASH_ProgramWord(BOOTLOADER_KEY_START_ADDRESS, BOOTLOADER_KEY_VALUE);
  FLASH_Lock();
} // End of SetKey()



//**************************************************************************************************
// Function      ResetKey()
// Description   Resets bootloader key
// Parameters    None
// RetVal        None
//**************************************************************************************************
void ResetKey()
{
  FLASH_Unlock();
  FLASH_ErasePage(BOOTLOADER_KEY_START_ADDRESS);
  FLASH_Lock();
} // End of ResetKey()



//**************************************************************************************************
// Function      ReadKey()
// Description   Reads bootloader key value
// Parameters    None
// RetVal        None
//**************************************************************************************************
uint32_t ReadKey()
{
  return (*(__IO uint32_t*) BOOTLOADER_KEY_START_ADDRESS);
} // End of ReadKey()



//**************************************************************************************************
// Function      main()
// Description   main function
// Parameters    None
// RetVal        None
//**************************************************************************************************
GPIO_InitTypeDef port;

int main()
{    
  if (ReadKey() == BOOTLOADER_KEY_VALUE)
  {
    ResetKey();
    __disable_irq();
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, MAIN_PROGRAM_START_ADDRESS);
      
    jumpAddress = *(__IO uint32_t*) (MAIN_PROGRAM_START_ADDRESS + 4);
    Jump_To_Application = (pFunction) jumpAddress;
    __set_MSP(*(__IO uint32_t*) MAIN_PROGRAM_START_ADDRESS);
    Jump_To_Application();
  }
  else
  {    
    NVIC_InitTypeDef NVIC_InitStructure;
    
    SD_Init();
        
    // SDIO Interrupt ENABLE
    NVIC_InitStructure.NVIC_IRQChannel = SDIO_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    // DMA2 Channel4 Interrupt ENABLE
    NVIC_InitStructure.NVIC_IRQChannel = DMA2_Channel4_5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    SD_GetCardInfo(&SDCard);
    SD_SelectDeselect((uint32_t) (SDCard.RCA << 16));
    
    FATFS   SDfs;
    FIL program;
    
    if(f_mount(&SDfs, "0", 1) == FR_OK)
    {
      uint8_t path[12] = "program.bin";
      path[11] = '\0';
          
      result = f_open(&program, (char*)path, FA_READ);
          
      if (result == FR_OK)
      {
        // Program        
        FLASH_Unlock();
        
        for(uint8_t i = 0; i < (NUM_OF_PAGES - MAIN_PROGRAM_PAGE_NUMBER); i++)
        {
           FLASH_ErasePage(MAIN_PROGRAM_START_ADDRESS + i * FLASH_PAGE_SIZE);
        }
        
        programBytesToRead = program.fsize;
        programBytesCounter = 0;
        currentAddress = MAIN_PROGRAM_START_ADDRESS;
        
        while ((programBytesToRead -  programBytesCounter) >= 512)
        {
          f_read(&program, readBuffer, 512, &readBytes);
          programBytesCounter += 512;
          
          for (uint32_t i = 0; i < 512; i += 4)
          {         
            FLASH_ProgramWord(currentAddress, *(uint32_t*)&readBuffer[i]);
              
            currentAddress += 4;
          }        
        }
          
        if (programBytesToRead != programBytesCounter)
        {
          f_read(&program, readBuffer, (programBytesToRead - programBytesCounter), &readBytes);
            
          for (uint32_t i = 0; i < (programBytesToRead - programBytesCounter); i += 4)
          {     
            FLASH_ProgramWord(currentAddress, *(uint32_t*)&readBuffer[i]);
                
            currentAddress += 4;
          }
          programBytesCounter = programBytesToRead;
        }      
        FLASH_Lock();
        
        f_close(&program);
        
        f_unlink((char*)path);
        
        NVIC_DisableIRQ(DMA2_Channel4_5_IRQn);
        NVIC_DisableIRQ(SDIO_IRQn);
        
        ResetKey();
        SetKey();         
        NVIC_SystemReset();
      }
      else
      {
        // Jump
        if (((*(uint32_t*)MAIN_PROGRAM_START_ADDRESS) & 0x2FFF0000 ) == 0x20000000)
        {
          ResetKey();
          SetKey();         
          NVIC_SystemReset();
        }
        else
        {
          NVIC_SystemReset();
        }
      }
    }
  }
    
  while(1)
  {
    
  }
  
  return 0;
} // End of main()



//**************************************************************************************************
// Local functions
//**************************************************************************************************

// None.



//**************************************************************************************************
// Interrupts
//**************************************************************************************************



//**************************************************************************************************
// Function      SDIO_IRQHandler()
// Description   Interrupt services for SDIO peripheral
// Parameters    None
// RetVal        None
//**************************************************************************************************
void SDIO_IRQHandler(void)
{
  /* Process All SDIO Interrupt Sources */
  SD_ProcessIRQSrc();
} // End of SDIO_IRQHandler() 



//**************************************************************************************************
// Function      DMA2_Channel4_5_IRQHandler()
// Description   Interrupt services for DMA peripheral
// Parameters    None
// RetVal        None
//**************************************************************************************************
void DMA2_Channel4_5_IRQHandler(void)
{
  /* Process DMA2 Stream3 or DMA2 Stream6 Interrupt Sources */
  SD_ProcessDMAIRQ();
} // End of DMA2_Channel4_5_IRQHandler() 

//****************************************** end of file *******************************************

