#ifndef __EXTI_H
#define __EXIT_H	 
#include "sys.h"
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
//������������󳤶�
#define PRESS_SIZE 50
#define EXTI_GPIOB_4

void EXTIX_Init(void);//�ⲿ�жϳ�ʼ��		
void SavePressLog(void);

extern char mqtt_mode[2]; //ͨ��mqtt���յ���ָ��
extern short All_State;	//Ϊ�˹�����������ʱ��ΰ�������������
extern volatile int Conce_PM2_5;       // PM2.5Ũ��
extern volatile int Conce_PM10;        // PM10Ũ��
extern volatile int AQI_2_5;
extern volatile int AQI_10;
extern volatile int AQI_Max;								//MAX(AQI_2_5,AQI_10)

extern unsigned long press_time;
extern unsigned char press_flag;
extern unsigned char wait_send_press;
extern int press_len;
extern char press_buf[PRESS_SIZE][2];
extern u32 press_time_log[PRESS_SIZE];
extern u16 press_C1[PRESS_SIZE];
extern u16 press_C2[PRESS_SIZE];
extern u16 press_AQI[PRESS_SIZE];
#endif

