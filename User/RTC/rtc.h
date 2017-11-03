#ifndef __RTC_H
#define __RTC_H	    
//Mini STM32������
//RTCʵʱʱ�� ��������			 
//����ԭ��@ALIENTEK
//2010/6/6

//�����ʹ�����õ����ʵ�ֶϵ�������Ч��RTC���ܣ�����ʹ��RCC_LSE��
#define RCC_LSE
//#define RCC_LSI
//#define RCC_HSE
#define UNIX_timestamp	//ʹ��UNIXʱ�����1970.1.1 8:00:00��ʼ

#include "sys.h"

//ʱ��ṹ��
typedef struct 
{
	vu8 hour;
	vu8 min;
	vu8 sec;			
	//������������
	vu16 w_year;
	vu8  w_month;
	vu8  w_date;
	vu8  week;
}_calendar_obj;					 
extern _calendar_obj calendar;	//�����ṹ��
extern _calendar_obj calendar_tmp;

extern u8 const mon_table[12];	//�·��������ݱ�
void Disp_Time(u8 x,u8 y,u8 size);//���ƶ�λ�ÿ�ʼ��ʾʱ��
void Disp_Week(u8 x,u8 y,u8 size,u8 lang);//��ָ��λ����ʾ����
u8 RTC_Init(u32 year, u8 month, u8 day, u8 hour, u8 min, u8 sec);        //��ʼ��RTC,����0,ʧ��;1,�ɹ�;
u8 Is_Leap_Year(u16 year);//ƽ��,�����ж�
u8 RTC_Alarm_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec);
u8 RTC_Get(void);         //����ʱ��   
u8 RTC_Get_Week(u16 year,u8 month,u8 day);
u8 RTC_Set(u16 syear,u8 smon,u8 sday,u8 hour,u8 min,u8 sec);//����ʱ��			 
u8 count2date(u32 current_count);
int timeout_SET(u32 syear, u8 smon, u8 sday, u8 hour, u8 min, u8 sec);	//����time_outʱ��
int UNIXtime2date(u32 UNIXtime);
#endif


