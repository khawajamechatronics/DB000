#include "DSHCHO.h"
#include "uart.h"
#include "stdio.h"

int Conce_HCHO = 0;				//��ȩŨ��
char DSHCHO_RX_BUF[10];
u16 check_short;
int data_equivalent;
u8 state;
u8 DSHCHO_RX_STA;

//static void (*DSHCHO_UartInit)(unsigned int);
//static void (*DSHCHO_UartSend)(unsigned char);

//void DSHCHO_Init(unsigned int bps,void (*uart_init)(unsigned int),void (*uart_send)(unsigned char))
//{
//	DSHCHO_UartInit = uart_init;
//	DSHCHO_UartSend = uart_send;
//	DSHCHO_UartInit(bps);
//}


void Recive_HCHO(u8 Res)
{
	int i;
//	u16 check_short;
//	int data_equivalent;
//	static u8 state;
//	static u8 DSHCHO_RX_STA;
	
	if(state == RECV_WAIT && Res == 0x42)             //������յĵ�һλ������0X42
	{
		state = RECV_42;              //��������ȷ���ڶ�λ�Ƿ���յ���0X4D
		DSHCHO_RX_STA = 0;    //����������ֵ��0��ʼ
		DSHCHO_RX_BUF[DSHCHO_RX_STA] = Res;
		DSHCHO_RX_STA++;
	}
	else if(state == RECV_42)
	{
		DSHCHO_RX_BUF[DSHCHO_RX_STA] = Res;
		if(DSHCHO_RX_STA == 1 && (Res!=0x4d)) state = RECV_WAIT; //�ڶ����ֽڲ���4d
		else if(DSHCHO_RX_STA == 2 && (Res!=0x8)) state = RECV_WAIT; //���������ֽ�������8
		else if(DSHCHO_RX_STA == 3 && (Res!=0x14)) state = RECV_WAIT; //�������಻�Ǽ�ȩ
		else if(DSHCHO_RX_STA >= 9) state = RECV_OVER;
		DSHCHO_RX_STA++;
	}
	else
	{
		state = RECV_WAIT; //�������������������������state�ȴ���������ͷ
	}
	
	if(state == RECV_OVER)	//��������������
	{
		check_short = 0;
		//����У��λ
		for(i = 0; i < 8; i++)
		{
			check_short += DSHCHO_RX_BUF[i];
		}
		if(check_short == ((DSHCHO_RX_BUF[8]<<8)+DSHCHO_RX_BUF[9]))
		{
			switch(DSHCHO_RX_BUF[5])	//�������ݵ���
			{
				case 1: data_equivalent = 1;	break;
				case 2: data_equivalent = 10;	break;
				case 3: data_equivalent = 100;	break;
				case 4: data_equivalent = 1000;	break;
				default: printf("data_equivalent is error\r\n"); return;
			}
			//Conce_HCHO = (float)((DSHCHO_RX_BUF[6]<<8)+DSHCHO_RX_BUF[7])/data_equivalent;
			Conce_HCHO = (DSHCHO_RX_BUF[6]<<8)+DSHCHO_RX_BUF[7];
			//printf("DSHCHO_RX_BUF = %x %x %x %x %x %x %x %x %x %x, Conce_HCHO = %d\r\n", DSHCHO_RX_BUF[0], DSHCHO_RX_BUF[1], DSHCHO_RX_BUF[2], DSHCHO_RX_BUF[3], DSHCHO_RX_BUF[4], DSHCHO_RX_BUF[5], DSHCHO_RX_BUF[6], DSHCHO_RX_BUF[7], DSHCHO_RX_BUF[8], DSHCHO_RX_BUF[9], Conce_HCHO);
			state = RECV_WAIT;
		}
		else
		{
			printf("HCHO check_short is error!\r\n");
			return;
		}
	}

	
}
	
void Send_DSHCHO_Cmd(void)
{
	int i = 0;
	u8 DS_HCHO_CMD[7] = {0x42, 0x4d, 0x01, 0x00, 0x00, 0x00, 0x90};
	for(i = 0; i < 7; i++)
	{
		while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);//�ȴ��������
		USART_SendData(USART3 , DS_HCHO_CMD[i]);
//		DSHCHO_UartSend(DS_HCHO_CMD[i]);
	}
}



