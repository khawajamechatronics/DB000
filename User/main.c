#define DEBUG_GSM_U3 1
//#define DEBUG_PM 1

#include "main.h"
#include "delay.h"
//#include "gsmlib.h"
//#include "gsmlib.cpp"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTPacket.h"  
#include "MyFifo.h"
#include "aqi.h"
//add for gprs
#include "uart1.h"
#include "uart3.h"
#include "interface.h"
#include "serialportAPI.h"
#include "sim800C.h"
//add for gprs
#include "cJSON.h"
#include "myJSON.h"
//add for ������
//#include "exti.h"
#include "key.h"
#include "rtc.h"
#include "stmflash.h"
//add for ����������
//#include "WS2812B.h"
#include "WS2812BNOP.h"

u16 U_ID[6];
char USART2_RX_BUF[10];
char power_state[4];
char mqtt_mode[2] = {"0"}; //ͨ��mqtt���յ���ָ��
unsigned int tim3_cnt = 0;	//Ϊ��ʵ��5���Ӷ�ʱ
unsigned int current_interval = CLOSE_INTERVAL;
volatile unsigned char fan_level = 0; //�Զ�ģʽ�µ��ٶȵ�λ
vu8 PM2_5_OK = 0;										//pm2.5�������Ƿ����ı�־
volatile int Conce_PM2_5 = 45;       // PM2.5Ũ��
volatile int Conce_PM10 = 55;        // PM10Ũ��
volatile int Max_PM = 55;
volatile int AQI_2_5 = 47;
volatile int AQI_10 = 46;
volatile int AQI_Max = 47;								//MAX(AQI_2_5,AQI_10)


short All_State = initialWAITOK;
char http_buf[512];	//GPRSģ��ͨ��httpЭ���ȡ������
char topic_group[30];
char deviceID[20] = "200033";
MQTTString topicString = MQTTString_initializer;
unsigned char mqtt_buf[MQTT_SEND_SIZE];   
int mqtt_buflen = sizeof(mqtt_buf); 
int len = 0;
int rc;
char payload[MQTT_SEND_SIZE] = "testmessage\n";	//MQTT������ȥ������
int payloadlen;

unsigned char send_flag = 0;
unsigned char ping_flag = 0;
unsigned char auto_flag = 0;
unsigned char internal_flag = 0;

int main()
{
	delay_init();	//��ʱ������ʼ��
  Fifo_All_Initialize();	//fifo��ʼ��
  RCC_Configuration(); //ʱ������
  NVIC_Configuration(); //�ж�Դ����
  GPIO_Configuration(); //io����

	RGB_Set(CUTDOWN, 4);	//�رտ���������
  USART2_Config();      //����2����
  UartBegin(115200, &USART1Conf, &U1_PutChar);				//����1����
  USART3Conf(115200);		//����3����
  TIM2_Init();					//ÿ1ms�ж�һ�εĶ�ʱ����������¼ʱ��
#ifndef ACTIVE_BEEP
	TIM1_Int_Init();			//�򿪶�ʱ��TIM1��������Դ��������PWM
#endif
	TIM3_Int_Init(9999, 7199);	//�򿪶�ʱ����ָ��ʱ�䷢�ʹ��������ݵ�������
	//EXTIX_Init();					//�������ⲿ�жϳ�ʼ��
	Panakey_Init();					//�������ⲿ�жϳ�ʼ��
	RTC_Init(2017, 1, 1, 0, 0, 0);						//ʵʱʱ�ӳ�ʼ�������������û��������ڲ���ʹ�á�
	//Timer4_init();	//TIM+DMA��ʽ���ƿ���������

	DBG("Open MUC!!!!");
	sprintf(DBG_BUF, "\r\n########### ��¼����: "__DATE__" - "__TIME__"\r\n");
	DBG(DBG_BUF);
	beep_on(BEEP_TIME);
	
	
	STMFLASH_Read(0x1ffff7e8,(u16*)U_ID,6);
	sprintf(DBG_BUF, "U_ID = %.4x-%.4x-%.4x-%.4x-%.4x-%.4x", U_ID[5],U_ID[4],U_ID[3],U_ID[2],U_ID[1],U_ID[0]);
	DBG(DBG_BUF);
//	while(1)
//	{
//		delay_ms(1000);
//		beep_on(200);
//	}

  while(1)
  {
	if(auto_flag == 1)
		{
			if(*mqtt_mode == 'A')
			{
				auto_mode((unsigned char *)&fan_level);
			}
			AirLEDControl();
			auto_flag = 0;
		}
		
    switch (All_State)
    {
      case initialWAITOK:
        Initial_GSM();
        break;
      case initialTCP:
        Initial_MQTT();
        break;
      case initialMQTT:
        MQTT_Sub0Pub1();
        break;
      case sendPM:
        Transmission_State();
        break;
      default:
        LedCountrol(0x0);
        break;
    }
  }
}

void RCC_Configuration(void)
{
  SystemInit();
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE);
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE);
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART3, ENABLE); //�򿪴��ڹܽ�ʱ��
}

void GPIO_Configuration(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

#ifndef SWIO_DEBUG	
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
	// �ı�ָ���ܽŵ�ӳ�� GPIO_Remap_SWJ_Disable SWJ ��ȫ���ã�JTAG+SW-DP��
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);
	// �ı�ָ���ܽŵ�ӳ�� GPIO_Remap_SWJ_JTAGDisable ��JTAG-DP ���� + SW-DP ʹ��
#endif
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;     //�źŵ�--PB3
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_3);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;     //������
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //��������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  //GPIO_SetBits(GPIOB, GPIO_Pin_14);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;     //LED1����--PB5
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_5);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;     //LED2����--PB6
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_6);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;     //LED3����--PB7
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_7);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;     //LED4����--PB8
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_8);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;			//LED5����--PB1
	//GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//�������
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������	
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_9);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;     //2����������
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;     //������
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_13);

//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;     //������
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //��������
//  GPIO_Init(GPIOB, &GPIO_InitStructure);
//  //GPIO_SetBits(GPIOB, GPIO_Pin_14);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;     //A0
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //��������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;     //A4
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;     //A5
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;     //A6
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;     //A7
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;     //GPRSģ��POWERKEY
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOB, GPIO_Pin_0); //PB0�ϵ�͵�ƽ
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;     //GPRSģ��VBAT
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //�������
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOA, GPIO_Pin_1); //PB1�ϵ�͵�ƽ

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;   //USART1 TX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //�����������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;   //USART1 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  //��������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //USART2 TX
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      //�����������
//  GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;     //A2
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //��������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;      //USART2 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   //��������
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//USART3 TX
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;//��������ٶ�50MHz
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;//���츴�����
  GPIO_Init(GPIOB, &GPIO_InitStructure); //����ʼ���õĽṹ��װ��Ĵ���

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;//USART3 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//GPIOģʽ��������
  GPIO_Init(GPIOB, &GPIO_InitStructure);//����ʼ���õĽṹ��װ��Ĵ���

}

void NVIC_Configuration(void)  //�ж����ȼ�NVIC����
{
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;  //��USART1�ж�ͨ��
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //���ȼ�
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //�����ȼ�
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  //ʹ���ж�ͨ��
  NVIC_Init(&NVIC_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;  //��USART2�ж�ͨ��
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  //���ȼ�
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  //�����ȼ�
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  //ʹ���ж�ͨ��
  NVIC_Init(&NVIC_InitStructure);

//	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
//	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;//�򿪸��ж�
//	NVIC_Init(&NVIC_InitStructure);

  /* Enable the TIM2 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3�ж�
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  //��ռ���ȼ�0��
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //�����ȼ�3��
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQͨ����ʹ��
  NVIC_Init(&NVIC_InitStructure);  //��ʼ��NVIC�Ĵ���

#ifdef EXTI_GPIOB_4
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;			//ʹ�ܰ���WK_UP���ڵ��ⲿ�ж�ͨ��
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//��ռ���ȼ�2�� 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;					//�����ȼ�3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//ʹ���ⲿ�ж�ͨ��
	NVIC_Init(&NVIC_InitStructure); 
#else
	NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;			//ʹ�ܰ���WK_UP���ڵ��ⲿ�ж�ͨ��
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//��ռ���ȼ�2�� 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;					//�����ȼ�3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//ʹ���ⲿ�ж�ͨ��
	NVIC_Init(&NVIC_InitStructure); 
#endif
}

void USART2_Config(void)
{

  USART_InitTypeDef USART_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE); // ʹ��USART��ʱ�Ӻ�GPIOA��ʱ�ӣ�ͬʱ��

  //initial UART2
  USART_InitStructure.USART_BaudRate = 9600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  //��Ӳ����
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART2, &USART_InitStructure );
  USART_ITConfig(USART2, USART_IT_RXNE, ENABLE); //���ڽ����ж�ʹ��
  //USART_ITConfig(USART1, USART_IT_TXE, ENABLE);    //ʹ�ܷ��ͻ�����ж�
  USART_Cmd(USART2, ENABLE);
}

void TIM1_Int_Init(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_OCInitTypeDef  TIM_OCInitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
//	TIM_BDTRInitTypeDef TIM_BDTRInitStructure;
	
	//ʹ��TIM1��ʱ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	
	//TIM1_CHN1 GPIO��ʼ��
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = 1000-1; // 72kHz 
	TIM_TimeBaseStructure.TIM_Prescaler = 72-1;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
	TIM_ARRPreloadConfig(TIM1, ENABLE);

	/* PWM1 Mode configuration: Channel4 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 100;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
	TIM_OC1Init(TIM1, &TIM_OCInitStructure);
	
//	 //��������
//	TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
//	TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
//	TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;
//	TIM_BDTRInitStructure.TIM_DeadTime = 0x90;  //�������������С0-0xff
//	TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;
//	TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_High;
//	TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
//	TIM_BDTRConfig(TIM1, &TIM_BDTRInitStructure);
	
	TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE ); //ʹ��ָ����TIM3�ж�,��������ж�
	
	//TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable); //ʹ��Ԥװ�ؼĴ���
	//TIM_SetCompare4(TIM1,300);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);	//TIM1_OCͨ�����PWM��һ��Ҫ�ӣ�
	//TIM_Cmd(TIM1, ENABLE); //ʹ�� TIM1
}

void TIM3_Int_Init(u16 arr, u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //ʱ��ʹ��

  //��ʱ��TIM3��ʼ��
  TIM_TimeBaseStructure.TIM_Period = arr; //��������һ�������¼�װ�����Զ���װ�ؼĴ������ڵ�ֵ
  TIM_TimeBaseStructure.TIM_Prescaler = psc; //����������ΪTIMxʱ��Ƶ�ʳ�����Ԥ��Ƶֵ
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //����ʱ�ӷָ�:TDTS = Tck_tim
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM���ϼ���ģʽ
  TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //����ָ���Ĳ�����ʼ��TIMx��ʱ�������λ

  TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE ); //ʹ��ָ����TIM3�ж�,��������ж�

  TIM_Cmd(TIM3, ENABLE);  //ʹ��TIMx
}

void beep_on(unsigned int time)
{
#ifdef ACTIVE_BEEP
	GPIO_SetBits(GPIOB, GPIO_Pin_13);
	delay_ms(time);
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
#else
	static int time_count = 35;
	sprintf(DBG_BUF, "time_count = %d", time_count);
	DBG(DBG_BUF);
	//TIM_SetCompare1(TIM1,time_count);
	TIM_PrescalerConfig(TIM1, time_count, TIM_PSCReloadMode_Immediate);
	TIM_CtrlPWMOutputs(TIM1, ENABLE);	//TIM1_OCͨ�����PWM��һ��Ҫ�ӣ�
	TIM_Cmd(TIM1, ENABLE); //ʹ�� TIM1
	delay_ms(time);
	TIM_PrescalerConfig(TIM1, time_count-5, TIM_PSCReloadMode_Immediate);
	delay_ms(time);
//	TIM_PrescalerConfig(TIM1, time_count, TIM_PSCReloadMode_Immediate);
//	delay_ms(time);
	TIM_CtrlPWMOutputs(TIM1, DISABLE);	//TIM1_OCͨ�����PWM��һ��Ҫ�ӣ�
	TIM_Cmd(TIM1, DISABLE); //ʹ�� TIM1
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
//	if(time_count >= 1000) time_count = 0;
//	else time_count += 100;
#endif
}

void restart_MCU(void)
{
  closeTCP();
  GSM_restart();
//  delay(1000);
//  __set_FAULTMASK(1);
//  NVIC_SystemReset();
	VBAT = 1;
	beep_on(1000);
	VBAT = 0;
	All_State = initialWAITOK;
	return;
}

void Initial_GSM()
{
  DBG("test!\n");
  while(0 == GSMInit(HOST_NAME, HOST_PORT, http_buf)) GSM_restart();
  //while(0 == HttpInit(http_buf));
  //DBG(http_buf);
  All_State = initialTCP;
}

void Initial_MQTT()
{

  MQTTPacket_connectData mqtt_data = MQTTPacket_connectData_initializer;
  mqtt_data.clientID.cstring = deviceID;
  mqtt_data.keepAliveInterval = PING_SET;
  mqtt_data.cleansession = 1;
  mqtt_data.username.cstring = deviceID;
  mqtt_data.password.cstring = "testpassword";
  //for will message
  mqtt_data.willFlag = 1;
  sprintf(topic_group, "clients/%s/state", deviceID);
  sprintf(DBG_BUF, "willtopic = %s", topic_group);
  DBG(DBG_BUF);
  mqtt_data.will.topicName.cstring = topic_group;
  mqtt_data.will.message.cstring = "0";
  mqtt_data.will.qos = 1;
  mqtt_data.will.retained = 1;

  len = MQTTSerialize_connect(mqtt_buf, mqtt_buflen, &mqtt_data);  //��仰��ʼMQTT�����ӣ����ǲ�ֱ�Ӻͷ��ͺ������������Ǵ浽һ��buf���棬�ٴ�buf���淢��
  sim800C_send(mqtt_buf, len);

  sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);	//sim800C_recv��ʵ���˽����ݴ���fifo3�Ĺ���
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
	sprintf(DBG_BUF, "rc = %d", rc);
	DBG(DBG_BUF);
  if ( rc == CONNACK)   //����ѻ�ȡ���ݵ�ָ�봫�˽�ȥ������
  {
    unsigned char sessionPresent, connack_rc;

    if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, mqtt_buf, mqtt_buflen) != 1 || connack_rc != 0)
    {
      DBG("MQTT CONNACK1 FAILED!");
      restart_MCU();
			return;
    }
    else
    {
      All_State = initialMQTT;
      DBG("MQTT CONNACK OK!");
    }
  }
  else
  {
    //failed ???
    DBG("MQTT CONNACK2 FAILED!");
    restart_MCU();
		return;
  }
}

void MQTT_Sub0Pub1()
{

  int msgid = 1;
  int req_qos = 0;

  //��������
  sprintf(topic_group, "SHAir/%s/get", deviceID);
  sprintf(DBG_BUF, "subtopic = %s", topic_group);
  DBG(DBG_BUF);
  topicString.cstring = topic_group;
  len = MQTTSerialize_subscribe(mqtt_buf, mqtt_buflen, 0, msgid, 1, &topicString, &req_qos);
  //������Щ������ֱ�ӷ��ͣ�����ͨ���Ȼ�ȡbuffer���������ֶ����ͳ�ȥ
  sim800C_send(mqtt_buf, len);

  sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
  if (rc == SUBACK)  /* wait for suback */ //��������������
  {
    unsigned short submsgid;
    int subcount;
    int granted_qos;

    rc = MQTTDeserialize_suback(&submsgid, 1, &subcount, &granted_qos, mqtt_buf, mqtt_buflen);
    if (granted_qos != 0)
    {
      //wrong
      DBG("MQTT SUBACK1 FAILED!");
      restart_MCU();
			return;
    }
    else
    {
      DBG("MQTT SUBACK OK!");
    }
  }
  else
  {
    DBG("MQTT SUBACK2 FAILED!");
    restart_MCU();
		return;
  }
	
	//����retain���ݣ�expiresAt��childLock
	sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
	if (rc == PUBLISH)
  {
    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short msgid;
    int payloadlen_in;
    unsigned char* payload_in;
    MQTTString receivedTopic;

		DBG("recive retain publish");
    rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
                                 &payload_in, &payloadlen_in, mqtt_buf, mqtt_buflen);
    //handle "payload_in" as data from the server
		sprintf(DBG_BUF, "retained = %d, msgid = %d, receivedTopic = %s", retained, msgid, receivedTopic.cstring);
		DBG(DBG_BUF);

    recv_mqtt(payload_in, payloadlen_in, payload, &payloadlen);
  }

  //����������ʾ
  if(!Public_Open(5))
  {
    DBG("PUBLIC OPEN ERROR");
    restart_MCU();
		return;
  }
  DBG("PUBLIC OPEN OK!");
	All_State = sendPM;

  //��������
  sprintf(topic_group, "SHAir/%s/update", deviceID);
  sprintf(DBG_BUF, "pubtopic = %s", topic_group);
  DBG(DBG_BUF);
  topicString.cstring = topic_group;
	//���������������
	beep_on(BEEP_TIME);
	delay(100);
	beep_on(BEEP_TIME);
	//��¼����ʱ��
	if(gprs_connect_cnt == 0)
		gprs_connect_time[gprs_connect_cnt] = UNIXtime2date(RTC_GetCounter());
	else
		gprs_connect_time[gprs_connect_cnt] = RTC_GetCounter() - break_time;
	gprs_connect_cnt++;
	if(gprs_connect_cnt >= GPRS_STATE_TIME_SIZE)	gprs_connect_cnt = 0;
	//���Ͷ����ڼ����������¼�����û�в�������������mode���ݡ�
	SendJson(CONNECTION_MODE);
	//�����ŷ��͵���λ����Ϣ��
	SendJson(GEO_MODE);
}

int Public_Open(int time)
{
  int i = 0;
	unsigned char dup = 0;
  int qos = 1;
  unsigned char retain = 1;
	unsigned short packedid = 1;	//PUBLISH��QoS ���� 0�����Ʊ��� �������һ������� 16 λ���ı�ʶ����Packet Identifier��

  for(i = 0; i < time; i++)
  {
    sprintf(topic_group, "clients/%s/state", deviceID);
    sprintf(DBG_BUF, "opentopic = %s", topic_group);
    DBG(DBG_BUF);
    topicString.cstring = topic_group;
    sprintf(payload, "1");
    //strcpy(payload, http_buf);
    payloadlen = strlen(payload);
    len = MQTTSerialize_publish(mqtt_buf, mqtt_buflen, dup, qos, retain, packedid, topicString, (unsigned char*)payload, payloadlen);
    sim800C_send(mqtt_buf, len);

    sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
    rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
    sprintf(DBG_BUF, "PUBLIC OPEN rc = %d", rc);
    DBG(DBG_BUF);
    if(rc == PUBACK) return 1;
		else dup = 1;	//��� DUP ��־������Ϊ 1����ʾ�������һ����ǰ����������ط���
  }

  return 0;
}

void Transmission_State()
{
	//���Ͷ����ڼ���������¼
	if(wait_send_press == 1)
	{
		wait_send_press = 0;
		DBG("send press!");
		SendJson(PRESS_MODE);
	}
	
	//����������
	if(ping_flag == 1)
	{
		ping_flag = 0;
		if(!SendPingPack(5))
		{
			DBG("pingpack is FAILED!");
			//��¼����ʱ��
			break_time = RTC_GetCounter();
			gprs_break_time[gprs_break_cnt] = UNIXtime2date(break_time);
			//gprs_break_time[gprs_break_cnt] = RTC_GetCounter();
			
			gprs_break_cnt++;
			if(gprs_break_cnt >= GPRS_STATE_TIME_SIZE)	gprs_break_cnt = 0;
			//����GPRSģ��
			restart_MCU();
			return;
		}
	}

  /* transport_getdata() has a built-in 1 second timeout,
  your mileage will vary */
  sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
  if (rc == PUBLISH)
  {
    unsigned char dup;
    int qos;
    unsigned char retained;
    unsigned short msgid;
    int payloadlen_in;
    unsigned char* payload_in;
    MQTTString receivedTopic;

    rc = MQTTDeserialize_publish(&dup, &qos, &retained, &msgid, &receivedTopic,
                                 &payload_in, &payloadlen_in, mqtt_buf, mqtt_buflen);
    //handle "payload_in" as data from the server

    recv_mqtt(payload_in, payloadlen_in, payload, &payloadlen);
		//���ؽ��յ�������
		len = MQTTSerialize_publish(mqtt_buf, mqtt_buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
    sim800C_send(mqtt_buf, len);
  }

	if(internal_flag == 1)	//internal_flag������send_flag֮ǰ���������������һλ��Ϊ0
	{
		internal_flag = 0;
				
		eATCSQ(&sim_csq);	//��ȡ��ǰcsqֵ		
		if(send_flag == 1) CSQ[(current_interval / COUNT_INTERVAL) - 1] = sim_csq;	//��Ϊtim3_cnt�����ж��м���ģ���send_flag=1ʱtim3_cnt�������㣬���ֻ����current_interval��ֵ����λ���һ�����ݵ�λ��
		else CSQ[(tim3_cnt / COUNT_INTERVAL) - 1] = sim_csq;
	}
	
  if(send_flag == 1)
  {
		send_flag = 0;
		SendJson(SENDDATA_MODE);
  }
}

void USART2_IRQHandler(void)//����2�жϷ������  PM2.5
{
  uint8_t Res;

  static char start = 0;
  static uint16_t USART2_RX_STA;

  if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)//�����ж�
  {
    Res = USART_ReceiveData(USART2);    //��ȡ���յ�������
    if(Res == 0x42)             //������յĵ�һλ������0X42
    {
      USART2_RX_STA = 0;    //����������ֵ��0��ʼ
      start = 1;              //��������ȷ���ڶ�λ�Ƿ���յ���0X4D
    }

    if(start == 1)
    {
      switch (USART2_RX_STA)
      {
        case 1:
          USART2_RX_BUF[0] = Res ;
          break;      //����0x4D
        case 12:
          USART2_RX_BUF[1] = Res ;
          break;      //���մ���������PM2.5��8λ
        case 13:
          USART2_RX_BUF[2] = Res ;
          break;      //���մ���������PM2.5��8λ
        case 14:
          USART2_RX_BUF[3] = Res ;
          break;      //���մ���������PM10��8λ
        case 15:
          USART2_RX_BUF[4] = Res ;
          break;      //���մ���������PM10��8λ
        default:
          break;
      }
      USART2_RX_STA++;

      if(USART2_RX_STA > 15 && (0x4D == USART2_RX_BUF[0]))
      {
        start  = 0;

        USART2_RX_STA = 0;              //��static���͸���ֵ

        Conce_PM2_5 = (USART2_RX_BUF[1] << 8) + USART2_RX_BUF[2];
        Conce_PM10  = (USART2_RX_BUF[3] << 8) + USART2_RX_BUF[4];
        Max_PM = (Conce_PM2_5 > Conce_PM10) ? Conce_PM2_5 : Conce_PM10;
				PM2_5_OK = 1;
				AQI_Count(Conce_PM2_5, Conce_PM10, (int *)&AQI_2_5, (int *)&AQI_10, (int *)&AQI_Max);
#ifdef DEBUG_PM
        sprintf(DBG_BUF, "pm2.5=%d,pm10=%d\n", Conce_PM2_5, Conce_PM10);
        DBG(DBG_BUF);
#endif

      }
    }
  }
}

//��ʱ��3�жϷ������
void TIM3_IRQHandler(void)   //TIM3�ж�
{
//  static unsigned int tim3_cnt = 0;	//Ϊ��ʵ��5���Ӷ�ʱ

  if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  //���TIM3�����жϷ������
  {
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //���TIMx�����жϱ�־
		
		//GPRS�ź�ָʾ��
		if(All_State == sendPM)	SIGNAL_LED = 0;
		else	SIGNAL_LED = !SIGNAL_LED;
		
		if(tim3_cnt % AUTOMODE_INTERVAL == 0)
		{
			auto_flag = 1;
		}

    if(tim3_cnt % COUNT_INTERVAL == 0)
    {
			internal_flag = 1;	//���и�CSQ�����ݲ����ж��н��д�����Ϊֻ�е�����������ܻ�ȡCSQֵ�����һ�ȡCSQ�ĺ��������ӳپ�����Ҫ���ж���ִ��
			
			C1[(tim3_cnt / COUNT_INTERVAL) - 1] = Conce_PM2_5;
			C2[(tim3_cnt / COUNT_INTERVAL) - 1] = Conce_PM10;
			AQI1[(tim3_cnt / COUNT_INTERVAL) - 1] = AQI_2_5;
			AQI2[(tim3_cnt / COUNT_INTERVAL) - 1] = AQI_10;
			AQI[(tim3_cnt / COUNT_INTERVAL) - 1] = AQI_Max;
			L[(tim3_cnt / COUNT_INTERVAL) - 1] = fan_level;
    }
 
		if((tim3_cnt % PING_INTERVAL == 0) && (All_State == sendPM))
		{
			ping_flag = 1;
		}
		
    if((tim3_cnt >= current_interval) && (All_State == sendPM))
    {
      tim3_cnt = 0;
      send_flag = 1;
    }

    tim3_cnt++;
  }
}

void SendJson(u8 mode)
{
	group_json(mode);
	payloadlen = strlen(payload);
	len = MQTTSerialize_publish(mqtt_buf, mqtt_buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
	sim800C_send(mqtt_buf, len);
}

int SendPingPack(int times)
{
	int i = 0;
	for(i=0;i<times;i++)
	{
		memset(mqtt_buf, 0, sizeof(mqtt_buf));
		len = MQTTSerialize_pingreq(mqtt_buf, 100);
		sim800C_send(mqtt_buf, len);

		sim800C_recv(mqtt_buf, sizeof(mqtt_buf), 1000);	//sim800C_recv��ʵ���˽����ݴ���fifo3�Ĺ���
		rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
		if ( rc == PINGRESP)   //����ѻ�ȡ���ݵ�ָ�봫�˽�ȥ������
		{
			sprintf(DBG_BUF, "rc = %d, pingreq is successful!", rc);
			DBG(DBG_BUF);
			return 1;
		}
		else
		{
			sprintf(DBG_BUF, "rc = %d, pingreq is failed!", rc);
			DBG(DBG_BUF);
		}
	}
	return 0;
}

void recv_mqtt(unsigned char* recv_data, int data_len, char* return_data, int* return_len)
{
  char * mode_tmp;
  char * power_tmp;
	int rtc_count;
	int timeout_count;
  cJSON *mqtt_recv_root = NULL;
	cJSON *http_root = NULL;
	cJSON *RTC_obj = NULL;
	cJSON *timeout_obj = NULL;
	char * out;

  sprintf(DBG_BUF, "MQTT recive data: %s, data_len: %d", recv_data, data_len);
  DBG(DBG_BUF);

  mqtt_recv_root = cJSON_Parse((const char *)recv_data);
  if(mqtt_recv_root != NULL)
  {
    DBG("mqtt_recv_root is ok!");
		
		//��expiresAt��
		if(cJSON_GetObjectItem(mqtt_recv_root, "expiresAt") != NULL)
    {
			DBG("expiresAt is OK");
			if(cJSON_IsNumber(cJSON_GetObjectItem(mqtt_recv_root, "expiresAt")))
			{
				DBG("expiresAt is Number");
				//��ʱ��������ݸ�ʽ
				timeout_count = cJSON_GetObjectItem(mqtt_recv_root, "expiresAt")->valueint;
				if(timeout_count > 1502883485 && timeout_count < 2147483647)
				{
					sprintf(DBG_BUF, "expiresAt = %d", timeout_count);
					DBG(DBG_BUF);
					Set_Flash_TimeOut(timeout_count, FLASH_SAVE_ADDR); //����ʱʱ��д��stm32��flash�У�д���ַ����ȵ�ǰ����Ĵ�СҪ��
					Set_Flash_TimeOut(0xaaaaaaaa, FLASH_SAVE_ADDR+4);
				}
				else DBG("expiresAt number is Error");
			}
			else DBG("expiresAt not Number");
    }
		
		//��childLock��
		if(cJSON_GetObjectItem(mqtt_recv_root, "childLock") != NULL)
    {
			if(All_State == sendPM) beep_on(BEEP_TIME);
			DBG("childLock is OK");
			if(cJSON_IsTrue(cJSON_GetObjectItem(mqtt_recv_root, "childLock")))
			{
				DBG("childLock is true");
				s_Powerkey.ChildLock_flag = 1;
			}
			else if(cJSON_IsFalse(cJSON_GetObjectItem(mqtt_recv_root, "childLock")))
			{
				DBG("childLock is false");
				s_Powerkey.ChildLock_flag = 0;
			}
			else	DBG("childLock ERROR!!!");
    }

    //����powerָ��յ�power on�����Զ�ģʽ���յ�power off�رշ�����յ�������ϢҲ�رշ��
    if(cJSON_GetObjectItem(mqtt_recv_root, "power") != NULL)
    {
			http_root = cJSON_Parse(http_buf);	//ֻ���ڽ��յ�powerָ���ʱ��Ż᷵�ػ�վ��Ϣ
			
      power_tmp = cJSON_GetObjectItem(mqtt_recv_root, "power")->valuestring;
      strcpy(power_state, power_tmp);
      sprintf(DBG_BUF, "recive power is %s, power_state is %s", power_tmp, power_state);
      DBG(DBG_BUF);
      if(!strcmp(power_state, "on"))
      {
        strcpy(mqtt_mode, "A");
        sprintf(DBG_BUF, "power on, mqtt_mode = %s", mqtt_mode);
        DBG(DBG_BUF);
      }
      else if(!strcmp(power_state, "off"))
      {
        strcpy(mqtt_mode, "0");
        sprintf(DBG_BUF, "power off, mqtt_mode = %s", mqtt_mode);
        DBG(DBG_BUF);
      }
      else
      {
        strcpy(mqtt_mode, "0");
        sprintf(DBG_BUF, "power messge is error, mqtt_mode = %s", mqtt_mode);
        DBG(DBG_BUF);
      }
			ModeCountrol();
    }

    //����mode����
    if(cJSON_GetObjectItem(mqtt_recv_root, "mode") != NULL)
    {
      mode_tmp = cJSON_GetObjectItem(mqtt_recv_root, "mode")->valuestring;
      strcpy(mqtt_mode, mode_tmp);
      sprintf(DBG_BUF, "mode_tmp = %s, mqtt_mode = %s", mode_tmp, mqtt_mode);
      DBG(DBG_BUF);
			ModeCountrol();
    }
		
    //����fan_test����
    if(cJSON_GetObjectItem(mqtt_recv_root, "fan_test") != NULL)
    {
			char * fan_test;
      fan_test = cJSON_GetObjectItem(mqtt_recv_root, "fan_test")->valuestring;
      sprintf(DBG_BUF, "fan_test = %s", fan_test);
      DBG(DBG_BUF);
			FanTest(fan_test);
    }
		
		//����RTC����
		RTC_obj = cJSON_GetObjectItem(mqtt_recv_root, "RTC");
		if(RTC_obj != NULL)
    {
			//��ʱ��������ݸ�ʽ
			rtc_count = cJSON_GetObjectItem(mqtt_recv_root, "RTC")->valueint;
			//rtc_count += 28800;	//ʱ����Ǵ�1970-1-1 08:00:00��ʼ�ģ������ǵ��㷨�Ǵ�1970-1-1 00:00:00�������Ҫ������Ӧ������
			PWR_BackupAccessCmd(ENABLE);	//ʹ��RTC�ͺ󱸼Ĵ�������
			RTC_SetCounter(rtc_count);	//����RTC��������ֵ
			RTC_WaitForLastTask();	//�ȴ����һ�ζ�RTC�Ĵ�����д�������
    }
		
		//����time_out����
		timeout_obj = cJSON_GetObjectItem(mqtt_recv_root, "time_out");
		if(timeout_obj != NULL)
    {
			//��ʱ��������ݸ�ʽ
			timeout_count = cJSON_GetObjectItem(mqtt_recv_root, "time_out")->valueint;
			Set_Flash_TimeOut(timeout_count, FLASH_SAVE_ADDR); //����ʱʱ��д��stm32��flash�У�д���ַ����ȵ�ǰ����Ĵ�СҪ��
			Set_Flash_TimeOut(0xaaaaaaaa, FLASH_SAVE_ADDR+4);
			//timeout_count += 28800;	//ʱ����Ǵ�1970-1-1 08:00:00��ʼ�ģ������ǵ��㷨�Ǵ�1970-1-1 00:00:00�������Ҫ������Ӧ������
//			PWR_BackupAccessCmd(ENABLE);	//ʹ�ܺ󱸼Ĵ�������
//			BKP_WriteBackupRegister(BKP_DR2, timeout_count & 0xffff);	//��󱸼Ĵ���BKP_DR2��д�볬ʱʱ��ĵ�16λ
//			BKP_WriteBackupRegister(BKP_DR3, timeout_count>>16 & 0xffff);	//��󱸼Ĵ���BKP_DR2��д�볬ʱʱ��ĸ�16λ
    }
		
		//���ض�����¼����
		if(cJSON_GetObjectItem(mqtt_recv_root, "GPRS_RECORD") != NULL)
		{
			SendJson(GPRS_RECORD_MODE);
		}
		
		if(http_root != NULL)	cJSON_AddItemReferenceToObject(mqtt_recv_root, "geo", http_root);
		else DBG("http_root == NULL!");
		
		//���յ���JSON������ӻ�վ��λ������Ȼ���������أ����ƻ�Ӧ��Ϣ
		out = cJSON_Print(mqtt_recv_root);			
		strcpy(return_data, out);
    *return_len = strlen(payload);
		sprintf(DBG_BUF, "return_data = %s�� return_len = %d", return_data, *return_len);
		DBG(DBG_BUF);		
		
		//�����ͷ�out�Ŀռ䣬��������
		free(out);
  }
	else
	{
		DBG("mqtt_recv_root is NULL!");
		strcpy(return_data, "recive data is error!");
    *return_len = strlen(payload);
	}

  //�����ͷ�json�Ŀռ䣬��������
	cJSON_Delete(http_root);
  cJSON_Delete(mqtt_recv_root);
}

void FanTest(char * fan_test)
{
	int fan_tmp = 0;
	switch(*fan_test){
		case '0': fan_tmp = 0x0; break;
		case '1': fan_tmp = 0x1; break;
		case '2': fan_tmp = 0x2; break;
		case '3': fan_tmp = 0x3; break;
		case '4': fan_tmp = 0x4; break;
		case '5': fan_tmp = 0x5; break;
		case '6': fan_tmp = 0x6; break;
		case '7': fan_tmp = 0x7; break;
		case '8': fan_tmp = 0x8; break;
		case '9': fan_tmp = 0x9; break;
		case 'a': fan_tmp = 0xa; break;
		case 'b': fan_tmp = 0xb; break;
		case 'c': fan_tmp = 0xc; break;
		case 'd': fan_tmp = 0xd; break;
		case 'e': fan_tmp = 0xe; break;
		case 'f': fan_tmp = 0xf; break;
		default: fan_tmp = 0x0; DBG("fan_test is error!");
	}
	sprintf(DBG_BUF, "fan_tmp = %d, fan_test = %s", fan_tmp, fan_test);
	DBG(DBG_BUF);
	SetMotorLevel(fan_tmp);
	LedCountrol(fan_tmp);
}

void ModeCountrol(void)
{
  if(*mqtt_mode == '0')
  {		
		fan_level = 0;
    MotorCountrol(0); //�رշ��
    LedCountrol(0x0);
  }
  else if((*mqtt_mode > '0' && *mqtt_mode <= '9') || (*mqtt_mode == 'A'))
  {
    if(*mqtt_mode == 'A') 	//�Զ�ģʽ
    {
      if(Conce_PM2_5 >= 0 && Conce_PM2_5 < PM2_5_LEVEL1) fan_level = 1;
      else if(Conce_PM2_5 >= PM2_5_LEVEL1 & Conce_PM2_5 < PM2_5_LEVEL2) fan_level = 2;
      else if(Conce_PM2_5 >= PM2_5_LEVEL2) fan_level = 3;
      else
      {
        fan_level = 0;
        DBG("Conce_PM2_5 is error");
      }

      MotorCountrol(fan_level);
      LedCountrol(0x1);
    }
    else    //����ģʽ
    {
      fan_level = 0;  //�ر��Զ�ģʽ
      switch(*mqtt_mode)
      {
        case '1':
          fan_level = 1;
          break;
        case '2':
          fan_level = 2;
          break;
        case '3':
          fan_level = 3;
          break;
        case '4':
          fan_level = 4;
          break;
        case '0':
          fan_level = 0;
          break;
        default:
          fan_level = 0;
          break;
      }

      MotorCountrol(fan_level);
      LedCountrol(1 << (fan_level));
    }
  }
	
	//������ڴ���״̬��˯��ģʽ�¹رտ���������
	AirLEDControl();
}

void LedCountrol(unsigned short mode)
{
	beep_on(BEEP_TIME);
	
	AUTO_LED = (~(mode >> 0) & 1);
	SLEEP_LED = (~(mode >> 1) & 1);
	SPEED1_LED = (~(mode >> 2) & 1);
	SPEED2_LED = (~(mode >> 3) & 1);
	SPEED3_LED = (~(mode >> 4) & 1);
}

void SetMotorLevel(int cmd)
{
  GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)((cmd >> 0) & 1));
  GPIO_WriteBit(GPIOA, GPIO_Pin_5, (BitAction)((cmd >> 1) & 1));
  GPIO_WriteBit(GPIOA, GPIO_Pin_6, (BitAction)((cmd >> 2) & 1));
  GPIO_WriteBit(GPIOA, GPIO_Pin_7, (BitAction)((cmd >> 3) & 1));
}

void MotorCountrol(unsigned char level)
{
  switch(level)
  {
    case 1:
      SetMotorLevel(MOTORSPEED1);
      DBG("Set Motor speed is 1");
      break;
    case 2:
      SetMotorLevel(MOTORSPEED2);
      DBG("Set Motor speed is 2");
      break;
    case 3:
      SetMotorLevel(MOTORSPEED3);
      DBG("Set Motor speed is 3");
      break;
    case 4:
      SetMotorLevel(MOTORSPEED4);
      DBG("Set Motor speed is 4");
      break;
    default:
      SetMotorLevel(MOTORSPEED0);
      DBG("Set Motor is power down");
  }
}

void auto_mode(unsigned char *level)
{
  switch(*level)
  {
    case 1:
      if(AQI_Max > PM2_5_LEVEL1 + LEVEL_OFFSET)
      {
        (*level)++;
        MotorCountrol(*level);
        DBG("Auto Motor speed is 2");
      }
      break;
    case 2:
      if(AQI_Max > PM2_5_LEVEL2 + LEVEL_OFFSET)
      {
        (*level)++;
        MotorCountrol(*level);
        DBG("Auto Motor speed is 3");
      }
      else if(AQI_Max < PM2_5_LEVEL1 - LEVEL_OFFSET)
      {
        (*level)--;
        MotorCountrol(*level);
        DBG("Auto Motor speed is 1");
      }
      break;
    case 3:
      if(AQI_Max < PM2_5_LEVEL2 - LEVEL_OFFSET)
      {
        (*level)--;
        MotorCountrol(*level);
        DBG("Auto Motor speed is 2");
      }
      break;
  }
}

void AirLEDControl(void)
{
	if(PM2_5_OK == 0)
	{
		RGB_Set(WHITE, 4);
		return;	//����ж�pm2.5������û�������������ÿ�����������ʾ��ɫ���������ú���
	}
	if(*mqtt_mode == '0' || *mqtt_mode == '1')
	{
		RGB_Set(CUTDOWN, 4);
		//DBG("sleep mode close the airled!");
	}
	else if(AQI_Max <= PM2_5_LEVEL1)
	{
		//WS2812_send(colors[4], 4);
		RGB_Set(GREEN, 4);
		//DBG("GREEN");
	}
	else if(AQI_Max > PM2_5_LEVEL1 && AQI_Max <= PM2_5_LEVEL2)
	{
		//WS2812_send(colors[1], 4);
		RGB_Set(ORANGE, 4);
		//DBG("ORANGE");
	}
	else if(AQI_Max > PM2_5_LEVEL2)
	{
		//WS2812_send(colors[0], 4);
		RGB_Set(RED, 4);
		//DBG("RED");
	}
	else
	{
		RGB_Set(WHITE, 4);
		DBG("airled control is error!");
	}
}


