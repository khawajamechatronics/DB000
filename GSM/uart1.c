#include "uart1.h"
#include "interface.h"

char DBG_BUF[DBG_BUF_SIZE];

//UART function
//UART3 TxD GPIOB10   RxD GPIOB11
void USART1Conf(u32 baudRate)
{
	USART_InitTypeDef USART_InitSturct;//���崮��1�ĳ�ʼ���ṹ��

//USART3 Configure	
	USART_InitSturct.USART_BaudRate = baudRate;//������19200
	USART_InitSturct.USART_WordLength = USART_WordLength_8b;//���ݿ��8λ
	USART_InitSturct.USART_StopBits = USART_StopBits_1;//һ��ֹͣλ
	USART_InitSturct.USART_Parity = USART_Parity_No;//����żУ��
	USART_InitSturct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitSturct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//ʹ�ܷ��������
	USART_Init(USART1 , &USART_InitSturct);//����ʼ���õĽṹ��װ��Ĵ���	
	//USART1_INT Configure
	USART_ITConfig(USART1 , USART_IT_RXNE , ENABLE);//ʹ�ܽ����ж�
//	USART_ITConfig(USART3 , USART_IT_TXE , ENABLE);
	USART_Cmd(USART1 , ENABLE);//�򿪴���
	USART_ClearFlag(USART1 , USART_FLAG_TC);//�����һ�����ݷ���ʧ�ܵ�����
}

void U1_PutChar(u8 Data)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);//�ȴ��������
	USART_SendData(USART1 , Data);
}
void U1_PutStr(char *str)//����һ���ַ���
{
	while(*str != '\0')
	{
		while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);//�ȴ��������
		USART_SendData(USART1 , *str++);
	}
}

void U1_PutNChar(u8 *buf , u16 size)
{
  u8 i;
	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET); //��ֹ��һ�ֽڶ�ʧ
	for(i=0;i<size;i++)
	{
		 USART_SendData(USART1 , buf[i]);
		 while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);//�ȴ��������
	}
}

void U2_PutDbgStrln(char *str)//����һ���ַ���������
{
	while(*str != '\0')
	{
		while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);//�ȴ��������
		USART_SendData(USART2 , *str++);
	}		
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);//�ȴ��������
	USART_SendData(USART2 , '\r');
	while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET);//�ȴ��������
	USART_SendData(USART2 , '\n');
}

void U3_PutDbgStrln(char *str)//����һ���ַ���������
{
	while(*str != '\0')
	{
		while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);//�ȴ��������
		USART_SendData(USART3 , *str++);
	}		
	while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);//�ȴ��������
	USART_SendData(USART3 , '\r');
	while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);//�ȴ��������
	USART_SendData(USART3 , '\n');
}
