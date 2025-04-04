#define F_CPU 72000000UL
//#include <stm32f10x_gpio.h>
//#include <stm32f10x_rcc.h>
//#include <stm32f10x_flash.h>
//#include <misc.h>
#include "OneWireSlave.h"
#include "Util.h"


extern "C" {
	volatile void SysTick_Handler(void);
	void EXTI0_IRQHandler(void);
}

OneWireSlave OneWire;

//bool InitSysClock()
//{
//	ErrorStatus HSEStartUpStatus;
//	RCC_DeInit();
//	RCC_HSEConfig(RCC_HSE_ON);
//	HSEStartUpStatus = RCC_WaitForHSEStartUp();
//	if (HSEStartUpStatus == SUCCESS)
//	{
//		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
//		FLASH_SetLatency(FLASH_Latency_2);
//		RCC_HCLKConfig(RCC_SYSCLK_Div1);
//		RCC_PCLK2Config(RCC_HCLK_Div1);
//		RCC_PCLK1Config(RCC_HCLK_Div2);
//		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
//		RCC_PLLCmd(ENABLE);
//		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
//		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
//		while (RCC_GetSYSCLKSource() != 0x08);
//	}
//	else return false; /* error HSE */
//	return true;
//}

void InitSysTickTimer(void) {
	SysTick->LOAD = F_CPU / 1000000 - 1;
	SysTick->VAL = F_CPU / 1000000 - 1;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}


volatile void SysTick_Handler(void) {
	delayInterrupt();
}

void EXTI0_IRQHandler(void) {
	OneWire.interrupt();
}



void setup()
{
	//if (!InitSysClock())__asm("bkpt 255"); //error system clock
	//InitSysTickTimer();
	//OneWire.init(GPIOB, GPIO_Pin_0, EXTI_Line0);
	OneWire.setROM(new uint8_t[6]{ 0x28, 0xB4, 0x99, 0x01, 0x00, 0x00 });
	OneWire.setIsEnable(true);


}

// Add the main program code into the continuous loop() function
void loop()
{
	if (OneWire.listener())
	{
		__asm("nop");
	}

}
