#include "stm32f10x.h"

#define RED 	0xFF00		//���
#define GREEN	0xFF0096	//�̹�
#define BLUE	0xFF			//����
#define WHITE	0xFFFFFF	//�׹�
#define ORANGE 0x5AFF00	//�ȹ�
#define CUTDOWN	0x000000	//�ر���ʾ
#define TESTCOLOR 0xAAAAAA	//������ɫ
//#define RED 	0xFF00		//���
//#define GREEN	0x9600FF	//�̹�
//#define BLUE	0xFF0000			//����
//#define WHITE	0xFFFFFF	//�׹�
//#define ORANGE 0xFF5A	//�ȹ�
//#define CUTDOWN	0x000000	//�ر���ʾ
//#define TESTCOLOR 0xAAAAAA	//������ɫ

void RGB_Set_Up(void);
void RGB_Set_Down(void);
void RGB_Set(u32 G8R8B8, int len);
void RGB_Rst(void);

