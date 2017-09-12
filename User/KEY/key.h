#include "sys.h"

/* �����˲�ʱ��50ms, ��λ10ms
 *ֻ��������⵽50ms״̬�������Ϊ��Ч����������Ͱ��������¼�
 */
#define BUTTON_FILTER_TIME         5
#define BUTTON_LONG_TIME         200                /* ����2�룬��Ϊ�����¼� */
#define KEY0 GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_4)
#define PRESS_SIZE 25

/*
        ÿ��������Ӧ1��ȫ�ֵĽṹ�������
        ���Ա������ʵ���˲��Ͷ��ְ���״̬�������
*/
typedef struct
{
  /* ������һ������ָ�룬ָ���жϰ����ַ��µĺ��� */
  unsigned char  (*IsKeyDownFunc)(void); /* �������µ��жϺ���,1��ʾ���� */

  unsigned char  Count;                        /* �˲��������� */
  unsigned char  FilterTime;                /* �˲�ʱ��(���255,��ʾ2550ms) */
  unsigned short LongCount;                /* ���������� */
  unsigned short LongTime;                /* �������³���ʱ��, 0��ʾ����ⳤ�� */
  unsigned char   State;                        /* ������ǰ״̬�����»��ǵ��� */
  unsigned char  KeyCodeUp;                /* ��������ļ�ֵ����, 0��ʾ����ⰴ������ */
  unsigned char  KeyCodeDown;        /* �������µļ�ֵ����, 0��ʾ����ⰴ������ */
  unsigned char  KeyCodeLong;        /* ���������ļ�ֵ����, 0��ʾ����ⳤ�� */
  unsigned char  RepeatSpeed;        /* ������������ */
  unsigned char  RepeatCount;        /* �������������� */
	unsigned char  IsLong;       			 /* �ж��ǲ��ǳ��� */
	unsigned char  timeout_flag;       /* ��ʱ��־ */
	unsigned char  ChildLock_flag;     /* ��ͯ����־ */
} BUTTON_T;

typedef enum
{
  KEY_NONE = 0,                        /* 0 ��ʾ�����¼� */

  KEY_DOWN_Power,                        /* ���������� */
  KEY_UP_Power,                        /* ���������� */
  KEY_LONG_Power,                        /* ���������� */

  KEY_DOWN_Power_TAMPER        /* ��ϼ���Power����WAKEUP��ͬʱ���� */
} KEY_ENUM;

extern BUTTON_T s_Powerkey;
extern char mqtt_mode[2]; //ͨ��mqtt���յ���ָ��
extern short All_State;	//Ϊ�˹�����������ʱ��ΰ�������������
extern volatile int Conce_PM2_5;       // PM2.5Ũ��
extern volatile int Conce_PM10;        // PM10Ũ��
extern volatile int AQI_2_5;
extern volatile int AQI_10;
extern volatile int AQI_Max;								//MAX(AQI_2_5,AQI_10)

extern unsigned char wait_send_press;
extern int press_len;
extern char press_buf[PRESS_SIZE][2];
extern u32 press_time_log[PRESS_SIZE];
extern u16 press_C1[PRESS_SIZE];
extern u16 press_C2[PRESS_SIZE];
extern u16 press_AQI[PRESS_SIZE];

void Panakey_Init(void);
void Pannelkey_Polling(void);