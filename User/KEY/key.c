#include "key.h"
#include "delay.h"
#include "uart1.h"
#include "string.h"
#include "main.h"
//#include "MyFifo.h"
#include "stdio.h"
#include "aqi.h"

unsigned char wait_send_press;
int press_len;
char press_buf[PRESS_SIZE][2];
u32 press_time_log[PRESS_SIZE];
u16 press_C1[PRESS_SIZE];
u16 press_C2[PRESS_SIZE];
u16 press_AQI[PRESS_SIZE];

BUTTON_T s_Powerkey;
//�Ƿ��а������½ӿں���
unsigned char  IsKeyDownUser(void)
{
  if (0 == KEY0) return 1;
  return 0;
}

void  PanakeyHard_Init(void)
{
  //GPIO_Init (POWER_KEY_PORT, POWER_KEY_PIN, GPIO_MODE_IN_FL_NO_IT);//power key
  //GPIO�ĳ�ʼ����main�����в���
}

void  PanakeyVar_Init(void)
{
  /* ��ʼ��USER����������֧�ְ��¡����𡢳��� */
  s_Powerkey.IsKeyDownFunc = IsKeyDownUser;                /* �жϰ������µĺ��� */
  s_Powerkey.FilterTime = BUTTON_FILTER_TIME;                /* �����˲�ʱ�� */
  s_Powerkey.LongTime = BUTTON_LONG_TIME;                        /* ����ʱ�� */
  s_Powerkey.Count = s_Powerkey.FilterTime / 2;                /* ����������Ϊ�˲�ʱ���һ�� */
  s_Powerkey.State = 0;                                                        /* ����ȱʡ״̬��0Ϊδ���� */
  s_Powerkey.KeyCodeDown = KEY_DOWN_Power;                        /* �������µļ�ֵ���� */
  s_Powerkey.KeyCodeUp = KEY_UP_Power;                               /* ��������ļ�ֵ���� */
  s_Powerkey.KeyCodeLong = KEY_LONG_Power;                        /* �������������µļ�ֵ���� */
  s_Powerkey.RepeatSpeed = 0;                                                /* �����������ٶȣ�0��ʾ��֧������ */
  s_Powerkey.RepeatCount = 0;                                                /* ���������� */
	s_Powerkey.IsLong = 0;
	s_Powerkey.timeout_flag = 0;
	s_Powerkey.ChildLock_flag = 0;
}

void Panakey_Init(void)
{
  PanakeyHard_Init();                /* ��ʼ���������� */
  PanakeyVar_Init();                /* ��ʼ������Ӳ�� */
}

void SavePressLog(void)
{
	//if (Fifo_canPush(&recv_fifo1)) Fifo_Push(&recv_fifo1, *mqtt_mode);
	//�����°������״̬�Ͱ��°�����ʱ���¼�������������
	if(All_State == sendPM) press_len = 0;
	else if(press_len >= PRESS_SIZE) press_len = PRESS_SIZE-1;	//����������ʱ���µ�����ֻ�滻ĩβ��һ������
	strcpy(press_buf[press_len], mqtt_mode);
	press_time_log[press_len] = RTC_GetCounter();
	press_C1[press_len] = Conce_PM2_5;
	press_C2[press_len] = Conce_PM10;			
	press_AQI[press_len] = AQI_Max;			
	press_len++;
	wait_send_press = 1;
}

void Pannelkey_Put(unsigned char KeyCode)
{
  // ����һ������ ���밴��ֵ
	if(KeyCode == KEY_DOWN_Power)
  {
    DBG("press!");
  }
  else if(KeyCode == KEY_UP_Power)
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
  else if(KeyCode == KEY_LONG_Power)
  {
    DBG("LONG PRESS");
		strcpy(mqtt_mode, "0");
		ModeCountrol();
		SavePressLog();
  }
	else
	{
		DBG("KeyCode is error!");
	}
}

/*
*********************************************************************************************************
*        �� �� ��: bsp_DetectButton
*        ����˵��: ���һ��������������״̬�����뱻�����Եĵ��á�
*        ��    �Σ������ṹ����ָ��
*        �� �� ֵ: ��
*********************************************************************************************************
*/
void Button_Detect(BUTTON_T *_pBtn)
{
  if (_pBtn->IsKeyDownFunc())
  {
    if (_pBtn->Count < _pBtn->FilterTime)
    {
      _pBtn->Count = _pBtn->FilterTime;
    }
    else if(_pBtn->Count < 2 * _pBtn->FilterTime)
    {
      _pBtn->Count++;
    }
    else
    {
      if (_pBtn->State == 0)
      {
        _pBtn->State = 1;

        /* ���Ͱ�ť���µ���Ϣ */
        if (_pBtn->KeyCodeDown > 0)
        {
          /* ��ֵ���밴��FIFO */
          Pannelkey_Put(_pBtn->KeyCodeDown);// ��¼�������±�־���ȴ��ͷ�

        }
      }

      if (_pBtn->LongTime > 0)
      {
        if (_pBtn->LongCount < _pBtn->LongTime)
        {
          /* ���Ͱ�ť�������µ���Ϣ */
          if (++_pBtn->LongCount == _pBtn->LongTime)
          {
            /* ��ֵ���밴��FIFO */
            Pannelkey_Put(_pBtn->KeyCodeLong);
						_pBtn->IsLong = 1;
          }
        }
        else
        {
          if (_pBtn->RepeatSpeed > 0)
          {
            if (++_pBtn->RepeatCount >= _pBtn->RepeatSpeed)
            {
              _pBtn->RepeatCount = 0;
              /* ��������ÿ��10ms����1������ */
              Pannelkey_Put(_pBtn->KeyCodeDown);

            }
          }
        }
      }
    }
  }
  else
  {
    if(_pBtn->Count > _pBtn->FilterTime)
    {
      _pBtn->Count = _pBtn->FilterTime;
    }
    else if(_pBtn->Count != 0)
    {
      _pBtn->Count--;
    }
    else
    {
      if (_pBtn->State == 1)
      {
        _pBtn->State = 0;

        /* ���Ͱ�ť�������Ϣ */
        if (_pBtn->KeyCodeUp > 0) /*�����ͷ�*/
        {
          /* ��ֵ���밴��FIFO */
					if(_pBtn->IsLong != 1) Pannelkey_Put(_pBtn->KeyCodeUp);
					_pBtn->IsLong = 0;
        }
      }
    }

    _pBtn->LongCount = 0;
    _pBtn->RepeatCount = 0;
  }
}
//����˵��: ������а�����10MS ����һ��
void Pannelkey_Polling(void)
{
  Button_Detect(&s_Powerkey);                /* USER �� */
}

