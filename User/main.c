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
//#include "uart3.h"
#include "interface.h"
#include "serialportAPI.h"
#include "sim800C.h"
//add for gprs
#include "cJSON.h"
#include "myJSON.h"
//add for 物理按键
//#include "exti.h"
#include "key.h"
#include "rtc.h"
#include "stmflash.h"
//add for 空气质量灯
//#include "WS2812B.h"
#include "WS2812BNOP.h"
//add for 蜂鸣器
#include "buzzer.h"
//add for 甲醛传感器
//#include "DSHCHO.h"
//add for PM2.5传感器
#include "PMS7003.h"

u16 U_ID[6];
char USART2_RX_BUF[10];
char power_state[4];
char mqtt_mode[2] = {"0"}; //通过mqtt接收到的指令
unsigned int tim3_cnt = 1;	//为了实现5分钟定时
unsigned int current_interval = CLOSE_INTERVAL;
volatile unsigned char fan_level = 0; //自动模式下的速度档位


short All_State = initialWAITOK;
char http_buf[512];	//GPRS模块通过http协议获取的数据
char topic_group[30];
char deviceID[20] = "200035";
MQTTString topicString = MQTTString_initializer;
unsigned char mqtt_buf[MQTT_SEND_SIZE];   
int mqtt_buflen = sizeof(mqtt_buf); 
int len = 0;
int rc;
char payload[MQTT_SEND_SIZE] = "testmessage\n";	//MQTT发布出去的数据
int payloadlen;

unsigned char send_flag = 0;
unsigned char ping_flag = 0;
unsigned char auto_flag = 0;
unsigned char internal_flag = 0;

int main()
{
	__enable_irq(); //使能所有中断
	
	delay_init();	//延时函数初始化
  Fifo_All_Initialize();	//fifo初始化
  RCC_Configuration(); //时钟配置
  NVIC_Configuration(); //中断源配置
  GPIO_Configuration(); //io配置

	RGB_Set(CUTDOWN, 4);	//关闭空气质量灯

  UartBegin(115200, &USART1Conf, &U1_PutChar);				//串口1配置
	USART2Conf(9600);      //串口2配置
  USART3Conf(9600);		//串口3配置
	//DSHCHO_Init(115200, &USART3Conf, &U3_PutChar);
  TIM2_Init();					//每1ms中断一次的定时器，用来记录时间
#ifndef ACTIVE_BEEP
	TIM1_Int_Init();			//打开定时器TIM1，产生无源蜂鸣器的PWM
#endif
	TIM3_Int_Init(9999, 7199);	//打开定时器，指定时间发送传感器数据到服务器
	//EXTIX_Init();					//物理按键外部中断初始化
	Panakey_Init();					//物理按键外部中断初始化
	RTC_Init(2017, 1, 1, 0, 0, 0);						//实时时钟初始化，用来限制用户超过租期不能使用。
	//Timer4_init();	//TIM+DMA方式控制空气质量灯

	DBG("APP V1.0!!!!");
	sprintf(DBG_BUF, "\r\n########### 烧录日期: "__DATE__" - "__TIME__"\r\n");
	DBG(DBG_BUF);
	beep_on(BEEP_TIME);
	
	STMFLASH_Read(0x1ffff7e8,(u16*)U_ID,6);
	sprintf(DBG_BUF, "U_ID = %.4x-%.4x-%.4x-%.4x-%.4x-%.4x", U_ID[5],U_ID[4],U_ID[3],U_ID[2],U_ID[1],U_ID[0]);
	DBG(DBG_BUF);
	
	//往FLASH_FIRMWARE_FLAG地址写1表示APP程序正常运行
	if(Flash_Read_Number(FLASH_FIRMWARE_FLAG) != 1) Flash_Write_Number(1, FLASH_FIRMWARE_FLAG);
	
	//给甲醛传感器发送命令接收返回数据
//	Send_DSHCHO_Cmd();
//	delay_ms(100);
//	sprintf(DBG_BUF, "DSHCHO_RX_BUF = %x %x %x %x %x %x %x %x %x %x, Conce_HCHO = %f", DSHCHO_RX_BUF[0], DSHCHO_RX_BUF[1], DSHCHO_RX_BUF[2], DSHCHO_RX_BUF[3], DSHCHO_RX_BUF[4], DSHCHO_RX_BUF[5], DSHCHO_RX_BUF[6], DSHCHO_RX_BUF[7], DSHCHO_RX_BUF[8], DSHCHO_RX_BUF[9], Conce_HCHO);
//	DBG(DBG_BUF);
	
//	while(1)
//	{
//		delay_ms(1000);
//		Send_DSHCHO_Cmd();
//		delay_ms(100);
//		sprintf(DBG_BUF, "DSHCHO_RX_BUF = %x %x %x %x %x %x %x %x %x %x, Conce_HCHO = %f", DSHCHO_RX_BUF[0], DSHCHO_RX_BUF[1], DSHCHO_RX_BUF[2], DSHCHO_RX_BUF[3], DSHCHO_RX_BUF[4], DSHCHO_RX_BUF[5], DSHCHO_RX_BUF[6], DSHCHO_RX_BUF[7], DSHCHO_RX_BUF[8], DSHCHO_RX_BUF[9], Conce_HCHO);
//		DBG(DBG_BUF);
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
//  SystemInit();
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
  RCC_APB2PeriphClockCmd( RCC_APB2Periph_USART1, ENABLE);
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART2, ENABLE);
  RCC_APB1PeriphClockCmd( RCC_APB1Periph_USART3, ENABLE); //打开串口管脚时钟
}

void GPIO_Configuration(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

#ifndef SWIO_DEBUG	
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_Disable, ENABLE);
	// 改变指定管脚的映射 GPIO_Remap_SWJ_Disable SWJ 完全禁用（JTAG+SW-DP）
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable , ENABLE);
	// 改变指定管脚的映射 GPIO_Remap_SWJ_JTAGDisable ，JTAG-DP 禁用 + SW-DP 使能
#endif
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;     //信号灯--PB3
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_3);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;     //物理按键
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //上拉输入
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  //GPIO_SetBits(GPIOB, GPIO_Pin_14);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;     //LED1控制--PB5
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_5);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;     //LED2控制--PB6
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_6);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;     //LED3控制--PB7
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_7);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;     //LED4控制--PB8
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_8);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;			//LED5控制--PB1
	//GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;			//复用输出
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出	
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_Pin_9);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;     //2空气质量灯
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;     //蜂鸣器
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_SetBits(GPIOB, GPIO_Pin_13);

//  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;     //物理按键
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //上拉输入
//  GPIO_Init(GPIOB, &GPIO_InitStructure);
//  //GPIO_SetBits(GPIOB, GPIO_Pin_14);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;     //A0
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //上拉输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;     //A4
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;     //A5
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;     //A6
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;     //A7
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;     //GPRS模块POWERKEY
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  GPIO_ResetBits(GPIOB, GPIO_Pin_0); //PB0上电低电平
	
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;     //GPRS模块VBAT
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;     //推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_SetBits(GPIOA, GPIO_Pin_1); //PA1上电低电平

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;   //USART1 TX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  //复用推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;   //USART1 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;  //浮空输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; //USART2 TX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      //复用推挽输出
  GPIO_Init(GPIOA, &GPIO_InitStructure);
//	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;     //A2
//  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     //上拉输入
//  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;      //USART2 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;   //浮空输入
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//USART3 TX
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;//设置最高速度50MHz
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;//推挽复用输出
  GPIO_Init(GPIOB, &GPIO_InitStructure); //将初始化好的结构体装入寄存器

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;//USART3 RX
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//GPIO模式悬浮输入
  GPIO_Init(GPIOB, &GPIO_InitStructure);//将初始化好的结构体装入寄存器

}

void NVIC_Configuration(void)  //中断优先级NVIC设置
{
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;  //打开USART1中断通道
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //优先级
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //子优先级
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  //使能中断通道
  NVIC_Init(&NVIC_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;  //打开USART2中断通道
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //优先级
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;  //子优先级
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  //使能中断通道
  NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;//打开该中断
	NVIC_Init(&NVIC_InitStructure);

  /* Enable the TIM2 gloabal Interrupt */
  NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;  //TIM3中断
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;  //先占优先级0级
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  //从优先级3级
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; //IRQ通道被使能
  NVIC_Init(&NVIC_InitStructure);  //初始化NVIC寄存器

#ifdef EXTI_GPIOB_4
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;			//使能按键WK_UP所在的外部中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//抢占优先级2， 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;					//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
	NVIC_Init(&NVIC_InitStructure); 
#else
	NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;			//使能按键WK_UP所在的外部中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;	//抢占优先级2， 
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;					//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//使能外部中断通道
	NVIC_Init(&NVIC_InitStructure); 
#endif
}

void TIM3_Int_Init(u16 arr, u16 psc)
{
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); //时钟使能

  //定时器TIM3初始化
  TIM_TimeBaseStructure.TIM_Period = arr; //设置在下一个更新事件装入活动的自动重装载寄存器周期的值
  TIM_TimeBaseStructure.TIM_Prescaler = psc; //设置用来作为TIMx时钟频率除数的预分频值
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; //设置时钟分割:TDTS = Tck_tim
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  //TIM向上计数模式
  TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); //根据指定的参数初始化TIMx的时间基数单位

  TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE ); //使能指定的TIM3中断,允许更新中断

  TIM_Cmd(TIM3, ENABLE);  //使能TIMx
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

  len = MQTTSerialize_connect(mqtt_buf, mqtt_buflen, &mqtt_data);  //这句话开始MQTT的连接，但是不直接和发送函数相连，而是存到一个buf里面，再从buf里面发送
  sim800C_send(mqtt_buf, len);

  mqtt_recv(mqtt_buf, sizeof(mqtt_buf), 1000);	//sim800C_recv内实现了将数据存入fifo3的功能
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
	sprintf(DBG_BUF, "rc = %d", rc);
	DBG(DBG_BUF);
  if ( rc == CONNACK)   //这里把获取数据的指针传了进去！！！
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

  //订阅主题
  sprintf(topic_group, "SHAir/%s/get", deviceID);
  sprintf(DBG_BUF, "subtopic = %s", topic_group);
  DBG(DBG_BUF);
  topicString.cstring = topic_group;
  len = MQTTSerialize_subscribe(mqtt_buf, mqtt_buflen, 0, msgid, 1, &topicString, &req_qos);
  //所有这些都不是直接发送，而是通过先获取buffer，我们再手动发送出去
  sim800C_send(mqtt_buf, len);

  mqtt_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
  rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
  if (rc == SUBACK)  /* wait for suback */ //会在这里阻塞？
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
	
	//接收retain数据：expiresAt和childLock
	mqtt_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
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

  //发布开机提示
  if(!Public_Open(5))
  {
    DBG("PUBLIC OPEN ERROR");
    restart_MCU();
		return;
  }
  DBG("PUBLIC OPEN OK!");
	All_State = sendPM;

  //发布主题
  sprintf(topic_group, "SHAir/%s/update", deviceID);
  sprintf(DBG_BUF, "pubtopic = %s", topic_group);
  DBG(DBG_BUF);
  topicString.cstring = topic_group;
	//连接上网络的声音
	beep_on(BEEP_TIME);
	delay(100);
	beep_on(BEEP_TIME);
	//记录连接时间
	if(gprs_connect_cnt == 0)
		gprs_connect_time[gprs_connect_cnt] = UNIXtime2date(RTC_GetCounter());
	else
		gprs_connect_time[gprs_connect_cnt] = RTC_GetCounter() - break_time;
	gprs_connect_cnt++;
	if(gprs_connect_cnt >= GPRS_STATE_TIME_SIZE)	gprs_connect_cnt = 0;
	//发送断网期间的物理按键记录，如果没有操作物理按键则发送mode数据。
	SendJson(CONNECTION_MODE);
	//紧接着发送地理位置信息。
	SendJson(GEO_MODE);
}

int Public_Open(int time)
{
  int i = 0;
	unsigned char dup = 0;
  int qos = 1;
  unsigned char retain = 1;
	unsigned short packedid = 1;	//PUBLISH（QoS 大于 0）控制报文 必须包含一个非零的 16 位报文标识符（Packet Identifier）

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

    mqtt_recv(mqtt_buf, sizeof(mqtt_buf), 1000);
    rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
    sprintf(DBG_BUF, "PUBLIC OPEN rc = %d", rc);
    DBG(DBG_BUF);
    if(rc == PUBACK) return 1;
		else dup = 1;	//如果 DUP 标志被设置为 1，表示这可能是一个早前报文请求的重发。
  }

  return 0;
}

void Transmission_State()
{
	//发送断网期间物理按键记录
	if(wait_send_press == 1)
	{
		wait_send_press = 0;
		DBG("send press!");
		SendJson(PRESS_MODE);
	}
	
	//发送心跳包
	if(ping_flag == 1)
	{
		ping_flag = 0;
		if(!SendPingPack(5))
		{
			DBG("pingpack is FAILED!");
			//记录断网时间
			break_time = RTC_GetCounter();
			gprs_break_time[gprs_break_cnt] = UNIXtime2date(break_time);
			//gprs_break_time[gprs_break_cnt] = RTC_GetCounter();
			
			gprs_break_cnt++;
			if(gprs_break_cnt >= GPRS_STATE_TIME_SIZE)	gprs_break_cnt = 0;
			//重启GPRS模块
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
		//返回接收到的命令
		len = MQTTSerialize_publish(mqtt_buf, mqtt_buflen, 0, 0, 0, 0, topicString, (unsigned char*)payload, payloadlen);
    sim800C_send(mqtt_buf, len);
  }

	if(internal_flag == 1)	//internal_flag必须在send_flag之前处理，否则数组最后一位就为0
	{
		internal_flag = 0;
				
		//eATCSQ(&sim_csq);	//获取当前csq值		
		
		if(send_flag == 1) //因为tim3_cnt是在中断中计算的，当send_flag=1时tim3_cnt立即清零，因此只能用current_interval的值来定位最后一个数据的位置
		{
			CSQ[(current_interval / COUNT_INTERVAL) - 1] = sim_csq;
		}
		else 
		{
			CSQ[(tim3_cnt / COUNT_INTERVAL) - 1] = sim_csq;
		}
	}
	
  if(send_flag == 1)
  {
		send_flag = 0;
		SendJson(SENDDATA_MODE);
  }
}

//定时器3中断服务程序
void TIM3_IRQHandler(void)   //TIM3中断
{
//  static unsigned int tim3_cnt = 0;	//为了实现5分钟定时

  if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)  //检查TIM3更新中断发生与否
  {
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update  );  //清除TIMx更新中断标志
		
		//GPRS信号指示灯
		if(All_State == sendPM)	SIGNAL_LED = 0;
		else	SIGNAL_LED = !SIGNAL_LED;
		
		if(tim3_cnt % AUTOMODE_INTERVAL == 0)
		{
			auto_flag = 1;
		}
		
//		if((tim3_cnt+1) % COUNT_INTERVAL == 0)	//要提前1S发送甲醛传感器的命令
//		{
//			Send_DSHCHO_Cmd(); //给甲醛传感器发送命令
//		}

    if(tim3_cnt % COUNT_INTERVAL == 0)
    {
			internal_flag = 1;	//还有个CSQ的数据不在中断中进行处理，因为只有当连上网络才能获取CSQ值，并且获取CSQ的函数里有延迟尽量不要在中断中执行
			
			//HCHO[(tim3_cnt / COUNT_INTERVAL) - 1] = Conce_HCHO;
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
	
	sprintf(DBG_BUF, "mode = %d, SendJson len = %d", mode, len);
	DBG(DBG_BUF);
}

int SendPingPack(int times)
{
	int i = 0;
	for(i=0;i<times;i++)
	{
		memset(mqtt_buf, 0, sizeof(mqtt_buf));
		len = MQTTSerialize_pingreq(mqtt_buf, 100);
		sim800C_send(mqtt_buf, len);

		mqtt_recv(mqtt_buf, sizeof(mqtt_buf), 1000);	//sim800C_recv内实现了将数据存入fifo3的功能
		rc = MQTTPacket_read(mqtt_buf, mqtt_buflen, fifo3readdata);
		if ( rc == PINGRESP)   //这里把获取数据的指针传了进去！！！
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
	char * firmware_cmd;
	int rtc_count;
	int timeout_count;
	char *ftp_server;
	char *ftp_username;
	char *ftp_passwd;
	char *ftp_filename;
	char *ftp_path;
  cJSON *mqtt_recv_root = NULL;
	cJSON *RTC_obj = NULL;
	cJSON *firmware_obj = NULL;
	char * out;
	
	//for debug
	u32 test_flag = 0;

  sprintf(DBG_BUF, "MQTT recive data: %s, data_len: %d", recv_data, data_len);
  DBG(DBG_BUF);

  mqtt_recv_root = cJSON_Parse((const char *)recv_data);
  if(mqtt_recv_root != NULL)
  {
    DBG("mqtt_recv_root is ok!");
		
		//处expiresAt据
		if(cJSON_GetObjectItem(mqtt_recv_root, "expiresAt") != NULL)
    {
			DBG("expiresAt is OK");
			if(cJSON_IsNumber(cJSON_GetObjectItem(mqtt_recv_root, "expiresAt")))
			{
				DBG("expiresAt is Number");
				//用时间戳的数据格式
				timeout_count = cJSON_GetObjectItem(mqtt_recv_root, "expiresAt")->valueint;
				if(timeout_count > 1502883485 && timeout_count < 2147483647)
				{
					sprintf(DBG_BUF, "expiresAt = %d", timeout_count);
					DBG(DBG_BUF);
					Flash_Write_Number(timeout_count, FLASH_SAVE_ADDR); //将超时时间写入stm32的flash中，写入地址必须比当前代码的大小要大
					Flash_Write_Number(0xaaaaaaaa, FLASH_SAVE_ADDR+4);
				}
				else DBG("expiresAt number is Error");
			}
			else DBG("expiresAt not Number");
    }
		
		//处childLock据
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

    //处理power指令，收到power on开启自动模式，收到power off关闭风机，收到其他信息也关闭风机
    if(cJSON_GetObjectItem(mqtt_recv_root, "power") != NULL)
    {			
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

    //处理mode数据
    if(cJSON_GetObjectItem(mqtt_recv_root, "mode") != NULL)
    {
      mode_tmp = cJSON_GetObjectItem(mqtt_recv_root, "mode")->valuestring;
      strcpy(mqtt_mode, mode_tmp);
      sprintf(DBG_BUF, "mode_tmp = %s, mqtt_mode = %s", mode_tmp, mqtt_mode);
      DBG(DBG_BUF);
			ModeCountrol();
    }
		
    //处理fan_test数据
    if(cJSON_GetObjectItem(mqtt_recv_root, "fan_test") != NULL)
    {
			char * fan_test;
      fan_test = cJSON_GetObjectItem(mqtt_recv_root, "fan_test")->valuestring;
      sprintf(DBG_BUF, "fan_test = %s", fan_test);
      DBG(DBG_BUF);
			FanTest(fan_test);
    }
		
		//处理RTC数据
		RTC_obj = cJSON_GetObjectItem(mqtt_recv_root, "RTC");
		if(RTC_obj != NULL)
    {
			//用时间戳的数据格式
			rtc_count = cJSON_GetObjectItem(mqtt_recv_root, "RTC")->valueint;
			//rtc_count += 28800;	//时间戳是从1970-1-1 08:00:00开始的，而我们的算法是从1970-1-1 00:00:00，因此需要加上相应的秒数
			PWR_BackupAccessCmd(ENABLE);	//使能RTC和后备寄存器访问
			RTC_SetCounter(rtc_count);	//设置RTC计数器的值
			RTC_WaitForLastTask();	//等待最近一次对RTC寄存器的写操作完成
    }
		
		//返回断网记录数据
		if(cJSON_GetObjectItem(mqtt_recv_root, "GPRS_RECORD") != NULL)
		{
			SendJson(GPRS_RECORD_MODE);
		}
		
		//处理firmware update数据
		firmware_obj = cJSON_GetObjectItem(mqtt_recv_root, "firmware");
		if(firmware_obj != NULL)
    {
			if(cJSON_GetObjectItem(firmware_obj, "cmd") != NULL)
			{
				firmware_cmd = cJSON_GetObjectItem(firmware_obj, "cmd")->valuestring;
				if(!strcmp(firmware_cmd, "update"))
				{
					//将ftp的参数写入到片内flash中
					if(cJSON_GetObjectItem(firmware_obj, "server") != NULL &&
					cJSON_GetObjectItem(firmware_obj, "username") != NULL &&
					cJSON_GetObjectItem(firmware_obj, "passwd") != NULL &&
					cJSON_GetObjectItem(firmware_obj, "filename") != NULL &&
					cJSON_GetObjectItem(firmware_obj, "path") != NULL)
					{
						ftp_server = cJSON_GetObjectItem(firmware_obj, "server")->valuestring;
						ftp_username = cJSON_GetObjectItem(firmware_obj, "username")->valuestring;
						ftp_passwd = cJSON_GetObjectItem(firmware_obj, "passwd")->valuestring;
						ftp_filename = cJSON_GetObjectItem(firmware_obj, "filename")->valuestring;
						ftp_path = cJSON_GetObjectItem(firmware_obj, "path")->valuestring;
						sprintf(DBG_BUF, "strlen = %d, %d, %d, %d, %d\r\n", strlen(ftp_server), strlen(ftp_username), strlen(ftp_passwd), strlen(ftp_filename), strlen(ftp_path));
						DBG(DBG_BUF);
						if(strlen(ftp_server)<32&&strlen(ftp_username)<32&&strlen(ftp_passwd)<32&&strlen(ftp_filename)<32&&strlen(ftp_path)<32)
						{
							Flash_Write_Str(FLASH_FTP_SERVER, (u8 *)ftp_server, FTP_PARAM_SIZE);
							Flash_Write_Str(FLASH_FTP_USERNAME, (u8 *)ftp_username, FTP_PARAM_SIZE);
							Flash_Write_Str(FLASH_FTP_PASSWD, (u8 *)ftp_passwd, FTP_PARAM_SIZE);
							Flash_Write_Str(FLASH_FTP_FILENAME, (u8 *)ftp_filename, FTP_PARAM_SIZE);
							Flash_Write_Str(FLASH_FTP_PATH, (u8 *)ftp_path, FTP_PARAM_SIZE);
												
							Flash_Write_Number(0, FLASH_FIRMWARE_FLAG);	//写0表示要更新固件
							test_flag = Flash_Read_Number(FLASH_FIRMWARE_FLAG);
							sprintf(DBG_BUF, "test_flag = %d", test_flag);
							DBG(DBG_BUF);
							//重启
							__disable_fault_irq();   
							NVIC_SystemReset();
							while(1);
						}
						else DBG("parame size >= 32!");
					}
					else DBG("parame is error!");
				}
				else DBG("cmd != update");
			}
			else DBG("cJSON_GetObjectItem(firmware_obj, \"cmd\") == NULL");

    }
		else	DBG("firmware_obj == NULL");
		
		//将收到的JSON数据立即返回，类似回应消息
		out = cJSON_Print(mqtt_recv_root);			
		strcpy(return_data, out);

		DBG(DBG_BUF);		
		
		//必须释放out的空间，否则会溢出
		free(out);
  }
	else
	{
		DBG("mqtt_recv_root is NULL!");
		strcpy(return_data, "recive data is error!");
	}
	
	*return_len = strlen(payload);
	sprintf(DBG_BUF, "return_data = %s， return_len = %d", return_data, *return_len);

  //必须释放json的空间，否则会溢出
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
    MotorCountrol(0); //关闭风机
    LedCountrol(0x0);
  }
  else if((*mqtt_mode > '0' && *mqtt_mode <= '9') || (*mqtt_mode == 'A'))
  {
    if(*mqtt_mode == 'A') 	//自动模式
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
    else    //控制模式
    {
      fan_level = 0;  //关闭自动模式
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
	
	//如果是在待机状态或睡眠模式下关闭空气质量灯
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
		return;	//如果判断pm2.5传感器没正常工作，就让空气质量灯显示白色，并跳出该函数
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


