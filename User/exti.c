#include "exti.h"
#include "delay.h"
#include "uart1.h"
#include "string.h"
#include "main.h"
//#include "MyFifo.h"
#include "stdio.h"
#include "aqi.h"


extern volatile unsigned long sys_tick;

unsigned long press_time = 0;
unsigned char press_flag = 0;
unsigned char wait_send_press;
int press_len;
char press_buf[PRESS_SIZE][2];
u32 press_time_log[PRESS_SIZE];
u16 press_C1[PRESS_SIZE];
u16 press_C2[PRESS_SIZE];
u16 press_AQI[PRESS_SIZE];

//////////////////////////////////////////////////////////////////////////////////	 
//������ֻ��ѧϰʹ�ã�δ��������ɣ��������������κ���;
//ALIENTEKս��STM32������
//�ⲿ�ж� ��������	   
//����ԭ��@ALIENTEK
//������̳:www.openedv.com
//�޸�����:2012/9/3
//�汾��V1.0
//��Ȩ���У�����ؾ���
//Copyright(C) ������������ӿƼ����޹�˾ 2009-2019
//All rights reserved									  
//////////////////////////////////////////////////////////////////////////////////   
//�ⲿ�ж�0�������
void EXTIX_Init(void)
{
 	EXTI_InitTypeDef EXTI_InitStructure;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);	//ʹ�ܸ��ù���ʱ��

#ifdef EXTI_GPIOB_4
	//GPIOA.0 �ж����Լ��жϳ�ʼ������   �½��ش���
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB,GPIO_PinSource4);

	EXTI_InitStructure.EXTI_Line=EXTI_Line4;	//������
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);	 	//����EXTI_InitStruct��ָ���Ĳ�����ʼ������EXTI�Ĵ���
#else
	//GPIOA.0 �ж����Լ��жϳ�ʼ������   �½��ش���
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA,GPIO_PinSource2);

	EXTI_InitStructure.EXTI_Line=EXTI_Line2;	//������
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;	
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);	 	//����EXTI_InitStruct��ָ���Ĳ�����ʼ������EXTI�Ĵ���
#endif
}

#ifdef EXTI_GPIOB_4
//�ⲿ�ж�4������� 
void EXTI4_IRQHandler(void)
{
	unsigned char it_start;
	it_start = GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4);
	delay_ms(10);//����
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4)==0 && it_start == 0)	 	 //WK_UP����
	{				 
		DBG("press");
		press_time = sys_tick;
		press_flag = 1;
		wait_send_press = 0;
	}
	if((GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4)==1) && (press_flag==1) && (it_start == 1))	 	 //WK_UP����
	{				 
		press_flag = 0;
		//DBG("press");
		press_time = sys_tick - press_time;
		if(press_time < 2000)	//�����Ƕ̰��Ĵ����������жϺʹ��������TIM2�ж��д���Ϊ��ʵ�ּ�ⳬ��2s��������Ӧ
		{
			DBG("short press");
			switch(*mqtt_mode)
			{
				case '0':
					strcpy(mqtt_mode,"A");
					break;
				case 'A':
					strcpy(mqtt_mode,"1");
					break;
				case '1':
					strcpy(mqtt_mode,"2");
					break;
				case '2':
					strcpy(mqtt_mode,"3");
					break;
				case '3':
					strcpy(mqtt_mode,"4");
					break;
				case '4':
					strcpy(mqtt_mode,"A");
					break;
			}
			ModeCountrol();
			SavePressLog();
		}
	}
	EXTI_ClearITPendingBit(EXTI_Line4); //���LINE0�ϵ��жϱ�־λ  
}
#else
//�ⲿ�ж�2������� 
void EXTI2_IRQHandler(void)
{
	delay_ms(10);//����
	if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_2)==0)	 	 //WK_UP����
	{				 
		DBG("press");
		press_time = sys_tick;
		press_flag = 1;
	}
	if((GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_2)==1) && (press_flag==1))	 	 //WK_UP����
	{				 
		press_flag = 0;
		//DBG("press");
		press_time = sys_tick - press_time;
		if(press_time < 2000)	//�����Ƕ̰��Ĵ����������жϺʹ��������TIM2�ж��д���Ϊ��ʵ�ּ�ⳬ��2s��������Ӧ
		{
			DBG("short press");
			switch(*mqtt_mode)
			{
				case '0':
					strcpy(mqtt_mode,"A");
					break;
				case 'A':
					strcpy(mqtt_mode,"1");
					break;
				case '1':
					strcpy(mqtt_mode,"2");
					break;
				case '2':
					strcpy(mqtt_mode,"3");
					break;
				case '3':
					strcpy(mqtt_mode,"4");
					break;
				case '4':
					strcpy(mqtt_mode,"A");
					break;
			}
			ModeCountrol();
			SavePressLog();
		}
	}
	EXTI_ClearITPendingBit(EXTI_Line2); //���LINE0�ϵ��жϱ�־λ  
}
#endif

void SavePressLog(void)
{
	//if (Fifo_canPush(&recv_fifo1)) Fifo_Push(&recv_fifo1, *mqtt_mode);
	//�����°������״̬�Ͱ��°�����ʱ���¼�������������
	if(press_len >= PRESS_SIZE || All_State == sendPM) press_len = 0;
	strcpy(press_buf[press_len], mqtt_mode);
	press_time_log[press_len] = RTC_GetCounter();
	AQI_Count(Conce_PM2_5, Conce_PM10, (int *)&AQI_2_5, (int *)&AQI_10, (int *)&AQI_Max);
	press_C1[press_len] = Conce_PM2_5;
	press_C2[press_len] = Conce_PM10;			
	press_AQI[press_len] = AQI_Max;			
	press_len++;
	wait_send_press = 1;
}
	


