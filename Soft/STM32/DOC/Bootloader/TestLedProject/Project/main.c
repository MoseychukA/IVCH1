#include "stm32f10x.h" 
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"

uint8_t currentState = 0;
GPIO_InitTypeDef port;
TIM_TimeBaseInitTypeDef timer;

void initAll()
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
   
    GPIO_StructInit(&port);
    
    port.GPIO_Mode = GPIO_Mode_Out_PP;
    port.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    port.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &port);

    TIM_TimeBaseStructInit(&timer);

    timer.TIM_Prescaler = 7200;

    timer.TIM_Period = 200;

    TIM_TimeBaseInit(TIM4, &timer);	
}

int main()
{
  __set_PRIMASK(1);
  NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0x0800A000);
  __set_PRIMASK(0);
  
  __enable_irq();
  
  initAll();
  
  TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
  TIM_Cmd(TIM4, ENABLE);
  NVIC_EnableIRQ(TIM4_IRQn);

  while(1)
  {
    
  }
  
  return 0;
}

void TIM4_IRQHandler()
{
  TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
  if (currentState == 0)
  {
    currentState = 1;
    GPIO_SetBits(GPIOC, GPIO_Pin_6);
  }
  else
  {
    currentState = 0;
    GPIO_ResetBits(GPIOC, GPIO_Pin_6);
  }
}

