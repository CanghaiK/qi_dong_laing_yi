#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fatfs.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"
#include "Function_interface.h"
#include "stm32f1xx_hal_flash_ex.h"
#include <stdio.h>


	int LCD_Max = 0;
	int LCD_Min = 1000000;
	int LCD_YuanDu;
	
	uint8_t LCD_INFO[2] = {0x5A,0xA5};
	uint8_t LCD_Len;
	uint8_t LCD_Fa = 0x82;
	uint8_t LCD_ADD[2];
	uint8_t LCD_Data2[2];
	
	uint8_t Send_8_data[10];				//��ҳ��ʾѡ�񼸸�ͨ��
	uint8_t MODE;									//����ģʽѡ��
	uint8_t Send_10_data[12];			//���������Բ��ֵ
	uint8_t Send_42_data[44];			//����������ʾʵʱʾ���붯̬ͼ
	uint8_t Send_126_data[132];		//���ݵ���������ʾ����ʷ����
	uint8_t Send_limit_data[16];	//У׼�͹������õ�������ֵ
	uint8_t LCD_Show_Gongcha;			//��ʾ��ʷ���õĹ���
	uint8_t LCD_Show_JiaoZhun;		//��ʾ��ʷ���õ�У׼
	uint8_t LCD_USB;							//USB������־λ
	uint8_t MODE_CASE[] = {0x5A,0XA5,0X07,0X82,0X00,0X84,0X5A,0X01,0X00,0X00};			//����ѡ������
	
	
	int Flash_Save_data[31];	//flash�洢��ʷ����
	int Flash_Limit_data[42];	//flash�洢����·У׼�Լ��������� ����˳��Ϊ ������У׼ ���޵�ѹ Kֵ ����У׼ ���޵�ѹ ���޹��� ���޹��
	int Flash_Up_Down_data[4];	//flash�洢�����һ�����õ�У׼ֵ������
	
	#define SAVE_DATA_ADD          0x08010000
	#define SAVE_LIMIT_ADD  			 0x08010800
	#define SAVE_UP_DOWN_ADD			 0x08011000
	
	FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */
	FIL MyFile;                   /* File object */
	
//	Calibration = 0;

//************************��Բ��***************************	
void Data_Extremum(uint8_t number)
{
	uint8_t num ;
	num = number - 1;
	if (LCD_Real_time_data_6[num] > LCD_Max)
	{
		LCD_Max = LCD_Real_time_data_6[num];
	}
	else if (LCD_Real_time_data_6[num] < LCD_Min)
	{
		LCD_Min = LCD_Real_time_data_6[num];
	}
	LCD_YuanDu = LCD_Max - LCD_Min;
	printf("���ֵΪ%d\r\n",LCD_Max);
	printf("��СֵΪ%d\r\n",LCD_Min);
	printf("Բ��ֵΪ%d\r\n",LCD_YuanDu);
}
//***************************�����Ҫ��ʾ����ֵ*********************************
void Get_Real_Time_data(void)
{
	uint8_t i;
	for(i = 0;i<6;i++)
	{
		if(MODE == 1)
		{
			LCD_Real_time_data_6[i] = LCD_Lower_Jiaozhun[i] +((ADC_sum_ave[i] - LCD_low[i])*K[i]);
		}
		if(MODE == 2)
		{
			LCD_Real_time_data_6[i] = LCD_Lower_Jiaozhun[i] +((LCD_low[i] - ADC_sum_ave[i])*K[i]);
		}
		if(MODE == 3)
		{
			LCD_Real_time_data_6[i] =  ADC_sum_ave[i];
		}
		printf("LCD_Real_time_data_6[5] = %d",LCD_Real_time_data_6[5]);
		Send_42_data[18+4*i] = (LCD_Real_time_data_6[i]>>24)&0xff ;
		Send_42_data[19+4*i] = (LCD_Real_time_data_6[i]>>16)&0xff ;                    
		Send_42_data[20+4*i] = (LCD_Real_time_data_6[i]>>8)&0xff ;
		Send_42_data[21+4*i] = (LCD_Real_time_data_6[i])&0xff ;
	}
}

//****************************flash�򿪲���**************************
void Erase_flash(uint32_t Address)
{
	FLASH_EraseInitTypeDef LIMIT_FLASH;             //������Ҫ��������Ϣ�ṹ��
	HAL_FLASH_Unlock();
	LIMIT_FLASH.TypeErase = FLASH_TYPEERASE_PAGES;  //����flashִ��ҳ��ֻ����������
	LIMIT_FLASH.PageAddress = Address;							//����Ҫ�����ĵ�ַ
	LIMIT_FLASH.NbPages =1;													//����Ҫ������ҳ��

	uint32_t PageError =0;
	HAL_FLASHEx_Erase(&LIMIT_FLASH,&PageError);//���ò�����������
}
//****************************��һ����д��flash***************************
void Flash_Write_words (uint32_t WriteAddr,uint32_t *pBuffer,uint8_t NumToWrite)
{
	uint8_t i;
	for(i = 0;i < NumToWrite;i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,WriteAddr,pBuffer[i]);
		WriteAddr+=4;
	}
}
//**************************��ȡ��flash�ж�ȡ������*****************************
uint32_t read_flash(uint32_t Address)  
{
	uint32_t pValue= *(__IO uint32_t*)(Address);
	return pValue;
}
//***************************��һ������flash�ж�ȡ*****************************
void Flash_Read_Words(uint32_t ReadAddr,uint32_t *pBuffer,uint8_t NumToRead)  
{
	uint8_t i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=read_flash(ReadAddr);//��ȡ1��32λ����.
		ReadAddr+=4;//ƫ��1����ַ.	
	}		
}
//******************************�������ݳ�ʼ��*******************************
void Send_Data_Iint(void)
{
		uint8_t i;
		//��ҳ����ʾѡ���˼���ͨ��
		Send_8_data[0] = LCD_INFO[0];
		Send_8_data[1] = LCD_INFO[1];
		Send_8_data[2] = 0x05;
		Send_8_data[3] = 0x82;
		Send_8_data[8] = 0x0D;
		Send_8_data[9] = 0x0A;
		//��������Ķ�̬ͼ��ʵʱʾ��
		Send_42_data[0] = LCD_INFO[0];
		Send_42_data[1] = LCD_INFO[1];
		Send_42_data[2] = 0x27;
		Send_42_data[3] = 0x82;
		Send_42_data[4] = 0x11;
		Send_42_data[5] = 0x00;
		Send_42_data[42] = 0x0D;
		Send_42_data[43] = 0x0A;
		//���������Բ��ֵ
		Send_10_data[0] =  LCD_INFO[0];
		Send_10_data[1] =  LCD_INFO[1];
		Send_10_data[2] =  0x07;
		Send_10_data[3] =  0x82;
		Send_10_data[4] =  0X11;
		Send_10_data[10] =  0x0D;
		Send_10_data[11] =  0x0A;
		//���ݵ����������ʷ����ֵ
		Send_126_data[0] = LCD_INFO[0];
		Send_126_data[1] = LCD_INFO[1];
		Send_126_data[2] = 0x7F;
		Send_126_data[3] = 0x82;
		Send_126_data[4] = 0x11;
		Send_126_data[5] = 0x30;
		Send_126_data[130] = 0x0D;
		Send_126_data[131] = 0x0A;
		//У׼�͹�������������ֵ
		Send_limit_data[0] = LCD_INFO[0];
		Send_limit_data[1] = LCD_INFO[1];
		Send_limit_data[2] = 0x0b;
		Send_limit_data[3] = 0x82;
		Send_limit_data[4] = 0x10;
		Send_limit_data[5] = 0x22;
		Send_limit_data[14] = 0x0D;
		Send_limit_data[15] = 0x0A;

		//���洢�����������ݶ�����
		Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
		for(i = 0;i<6;i++)
		{
		 LCD_Upper_Jiaozhun[i] =  Flash_Limit_data[7*i];    //У׼���ֵ
		 LCD_high[i]           =  Flash_Limit_data[7*i+1];	//У׼���ֵʱ�ĵ�ѹֵ
		 K[i]                  =  Flash_Limit_data[7*i+2];
		 LCD_Lower_Jiaozhun[i] =  Flash_Limit_data[7*i+3];	//У׼��Сֵ
		 LCD_low[i]            =  Flash_Limit_data[7*i+4];	//У׼��Сʱ�ĵ�ѹֵ
		 LCD_Upper_GongCha[i]  =  Flash_Limit_data[7*i+5];	//�������ֵ
		 LCD_Lower_GongCha[i]  =  Flash_Limit_data[7*i+6];	//������Сֵ
		}
		Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 
		for(i = 0;i<7;i++)
		{
			printf("Flash_Limit_data[%d] = %d\r\n",i,Flash_Limit_data[i]);
		}
		//�ƵƵ�ƽ�ø�
		HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1, GPIO_PIN_SET); 
		//485оƬ��ʼֵ�õ�
		HAL_GPIO_WritePin(GPIOC,GPIO_PIN_9, GPIO_PIN_RESET); 

		LCD_Shouye = 0;
		LCD_Daochu = 0;
		LCD_Show_Real_time_data_6 = 0;
		LCD_Jiaozhun = 0;
		LCD_Gongcha = 0;
		
		LCD_SaveDataShow = 0;
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		Channel_Choose = 0;
		Calibration = 0;
		LCD_Show_N_OF_Channels = 0;
		LCD_USB = 0;
}
//�������
void Touch_check(void)
{
	if(LCD_Need_check == 1)
	{
		int i;
		int j;
		for(i = 0;i<11;i++)
		{
			printf("%x ",USART3_BUFF[i]);
			if (USART3_BUFF[i] == 0x5A)
			{
				for(j = 0;j<11-i;j++)
				{
					usart3_buffer[j] = USART3_BUFF[i+j];
				}
				break;
			}
		}
		for(i = 0;i < 10;i++)
		{
			printf(" %x",usart3_buffer[i]);
		}
		//**************************************��ҳ********************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x00)	//������ҳ
		{
			printf("____������ҳ_______\r\n");
			LCD_Shouye = 1;
			LCD_Daochu = 0;
			LCD_Show_YuanDu = 0;
			LCD_Show_Real_time_data_6 = 0;
			LCD_DataSave = 0;
			LCD_Jiaozhun = 0;
			LCD_Gongcha = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			//ѡ����ʾ����
			switch (MODE){
				case 1: MODE_CASE[9] = 0X12;
					break;
				case 2: MODE_CASE[9] = 0X13;
					break;
				case 3: MODE_CASE[9] = 0X14;
					break;
				default:	break;
				}
			HAL_UART_Transmit(&huart3,MODE_CASE,sizeof(MODE_CASE),1000);
		}
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x70)	//�ھ�����
		{
			MODE = 1;
		}
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x72)	//�⾶����
		{
			MODE = 2;
		}
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x74)	//��ʾ��ѹ
		{
			MODE = 3;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x26)	//ѡ������ͨ��
		{
			printf("____ѡ������ͨ��______\r\n");
			LCD_N_OF_Channels = 2;
			LCD_Show_N_OF_Channels = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x28)	//ѡ���ĸ�ͨ��
		{
			printf("____ѡ���ĸ�ͨ��______\r\n");
			LCD_N_OF_Channels = 4;
			LCD_Show_N_OF_Channels = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x2a)	//ѡ������ͨ��
		{
			printf("____ѡ������ͨ��______\r\n");
			LCD_N_OF_Channels = 6;
			LCD_Show_N_OF_Channels = 1;
		}
		//**********************************����***********************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x02)  //�����������
		{
			printf("____�����������_____\r\n");
			LCD_Show_Real_time_data_6 =1;
			LCD_Show_YuanDu_Add[0]= 0x11;
			LCD_Shouye = 0;
			LCD_Daochu = 0;
			LCD_Show_YuanDu = 0;
			LCD_DataSave = 0;
			LCD_Jiaozhun = 0;
			LCD_Gongcha = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			printf("LCD_Show_Real_time_data_6 = %d\r\n",LCD_Show_Real_time_data_6);
				printf("LCD_Jiaozhun = %d",LCD_Jiaozhun);
				printf("LCD_Gongcha = %d\r\n",LCD_Gongcha);
				printf("LCD_Daochu = %d\r\n",LCD_Daochu);
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0a)	//��ʾͨ��һԲ��
		{
			printf("______ ��ʾͨ��һԲ�� ______\r\n");
			LCD_Show_YuanDu = 1;
			LCD_Show_YuanDu_Add[1]= 0x12;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0c)	//��ʾͨ����Բ��
		{
			printf("______ ��ʾͨ����Բ��\r\n");
			LCD_Show_YuanDu = 2;
			LCD_Show_YuanDu_Add[1]= 0x14;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0e)	//��ʾͨ����Բ��
		{
			printf("______ ��ʾͨ����Բ��\r\n");
			LCD_Show_YuanDu = 3;
			LCD_Show_YuanDu_Add[1]= 0x16;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x10)	//��ʾͨ����Բ��
		{
			printf("______ ��ʾͨ����Բ��\r\n");
			LCD_Show_YuanDu = 4;
			LCD_Show_YuanDu_Add[1]= 0x18;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x12)	//��ʾͨ����Բ��
		{
			printf("______ ��ʾͨ����Բ��\r\n");
			LCD_Show_YuanDu = 5;
			LCD_Show_YuanDu_Add[1]= 0x1a;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x14)	//��ʾͨ����Բ��
		{
			printf("______ ��ʾͨ����Բ��\r\n");
			LCD_Show_YuanDu = 6;
			LCD_Show_YuanDu_Add[1]= 0x1c;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x2c)	//�����������
		{
			printf("______ �����������\r\n");
			LCD_DataSave = 1;
		}
		//*********************************У׼************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x04)	//����У׼����
		{
			printf("______ ����У׼����\r\n");
			LCD_Jiaozhun = 1;
			
			LCD_Shouye = 0;
			LCD_Daochu = 0;
			LCD_Show_YuanDu = 0;
			LCD_Show_Real_time_data_6 = 0;
			LCD_DataSave = 0;
			LCD_Gongcha = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			printf("LCD_Show_Real_time_data_6 = %d\r\n",LCD_Show_Real_time_data_6);
				printf("LCD_Jiaozhun = %d",LCD_Jiaozhun);
				printf("LCD_Gongcha = %d\r\n",LCD_Gongcha);
				printf("LCD_Daochu = %d\r\n",LCD_Daochu);
		}
		//*******************************����***************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x06)
		{
			printf("______ ���빫�����\r\n");
			LCD_Gongcha = 1;
			LCD_Shouye = 0;
			LCD_Daochu = 0;
			LCD_Show_YuanDu = 0;
			LCD_Show_Real_time_data_6 = 0;
			LCD_DataSave = 0;
			LCD_Jiaozhun = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			printf("LCD_Show_Real_time_data_6 = %d\r\n",LCD_Show_Real_time_data_6);
				printf("LCD_Jiaozhun = %d",LCD_Jiaozhun);
				printf("LCD_Gongcha = %d\r\n",LCD_Gongcha);
				printf("LCD_Daochu = %d\r\n",LCD_Daochu);
		}
		//*********************************У׼ͨ������****************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x16)
		{
			printf("______ ѡ��ͨ��һ��������\r\n");
			Channel_Choose = 1;
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[0]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[0]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[0]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[0]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[3]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[3]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[3]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[3]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[5]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[5]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[5]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[5]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[6]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[6]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[6]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[6]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x18)
		{
			printf("______ ѡ��ͨ������������\r\n");
			Channel_Choose = 2;	
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[7]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[7]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[7]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[7]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[10]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[10]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[10]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[10]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[12]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[12]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[12]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[12]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[13]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[13]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[13]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[13]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x1a)
		{
			printf("______ ѡ��ͨ������������\r\n");
			Channel_Choose = 3;	
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[14]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[14]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[14]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[14]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[17]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[17]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[17]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[17]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[19]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[19]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[19]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[19]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[20]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[20]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[20]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[20]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x1c)
		{
			printf("______ ѡ��ͨ���Ľ�������\r\n");
			Channel_Choose = 4;	
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[21]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[21]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[21]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[21]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[24]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[24]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[24]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[24]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[26]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[26]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[26]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[26]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[27]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[27]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[27]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[27]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x1e)
		{
			printf("______ ѡ��ͨ�����������\r\n");
			Channel_Choose = 5;	
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[28]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[28]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[28]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[28]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[31]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[31]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[31]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[31]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[33]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[33]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[33]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[33]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[34]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[34]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[34]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[34]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x20)
		{
			printf("______ ѡ��ͨ������������\r\n");
			Channel_Choose = 6;	
			if(LCD_Jiaozhun == 1){
				Send_limit_data[6] = (Flash_Limit_data[35]>>24)&0xff;  
				Send_limit_data[7] = (Flash_Limit_data[35]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[35]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[35]&0xff; 
				Send_limit_data[10] = (Flash_Limit_data[38]>>24)&0xff; 
				Send_limit_data[11] =	(Flash_Limit_data[38]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[38]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[38]&0xff; 
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}else if(LCD_Gongcha == 1){
				Send_limit_data[6] = (Flash_Limit_data[40]>>24)&0xff; 
				Send_limit_data[7] = (Flash_Limit_data[40]>>16)&0xff; 	
				Send_limit_data[8] = (Flash_Limit_data[40]>>8)&0xff; 	
				Send_limit_data[9] = Flash_Limit_data[40]&0xff;
				Send_limit_data[10] = (Flash_Limit_data[41]>>24)&0xff;  
				Send_limit_data[11] =	(Flash_Limit_data[41]>>16)&0xff; 
				Send_limit_data[12] =	(Flash_Limit_data[41]>>8)&0xff; 
				Send_limit_data[13] = Flash_Limit_data[41]&0xff;
				HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			}
		}
		//********************************У׼��ť*************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x22)
		{
			printf("______ ��������У׼\r\n");
			int opp;
			for (opp = 0;opp<11;opp++)
			{
			printf(" %x",usart3_buffer[opp]);
			}
			Calibration = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x24)
		{
			printf("______ ��������У׼\r\n");
			Calibration = 2;
			int opp;
			for (opp = 0;opp<11;opp++)
			{
			printf(" %x",usart3_buffer[opp]);
			}
		}
		//*******************************���ݵ���*****************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x08)
		{
			printf("______ ���ݵ���\r\n");
			LCD_SaveDataShow = 1;
			
			LCD_Daochu = 1;
			LCD_Shouye = 0;
			LCD_Show_YuanDu = 0;
			LCD_Show_Real_time_data_6 = 0;
			LCD_DataSave = 0;
			LCD_Jiaozhun = 0;
			LCD_Gongcha = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			printf("LCD_Show_Real_time_data_6 = %d\r\n",LCD_Show_Real_time_data_6);
				printf("LCD_Jiaozhun = %d",LCD_Jiaozhun);
				printf("LCD_Gongcha = %d\r\n",LCD_Gongcha);
				printf("LCD_Daochu = %d\r\n",LCD_Daochu);
		}
		LCD_Need_check = 0;
	}
}
//***************************************���̵���**************************************************
void MSC_Application(void)
{
	uint8_t i;
	FRESULT res;
	uint32_t bytewritten;
	uint32_t wtext[4];
	if(f_mount(&USBDISKFatFs,(TCHAR const*)USBHPath, 0)!= FR_OK)
	{
		Error_Handler();
		printf("f_mount(&USBHFatFS,(TCHAR const*)USBHPath, 0)!= FR_OK\r\n");
	}
	else 
	{
		if(f_open(&MyFile,"History_DATA.TXT",FA_CREATE_ALWAYS|FA_WRITE)!= FR_OK)
		{
			Error_Handler();
			printf("f_open(&USBHFile,STM32.TXT,FA_CREATE_ALWAYS|FA_WRITE)!= FR_OK\r\n");
		}
		else
		{
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x21;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
			
			HAL_Delay(500);
			f_printf(&MyFile,"%s ","��һ������");
			for (i = 0;i<5;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[5]);
			f_printf(&MyFile,"%s ","�ڶ�������");
			for (i = 6;i<11;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[11]);
			f_printf(&MyFile,"%s ","����������");
			for (i = 12;i<17;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[17]);
			f_printf(&MyFile,"%s ","����������");
			for (i = 18;i<23;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[23]);
			f_printf(&MyFile,"%s ","����������");
			for (i = 24;i<29;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[29]);
			printf("д���ļ�\r\n");
			res = f_lseek(&MyFile, 0); 
			if(res != FR_OK)
      {
         /* 'STM32.TXT' file Write or EOF Error */
        Error_Handler();
				printf("���ݵ�������\r\n");
      }
      else
       {
          /* Close the open text file */
				 printf("�ر�����\r\n");
          f_close(&MyFile);
				 HAL_Delay(1000);
				Send_8_data[4] = 0x11;
				Send_8_data[5] = 0x21;
				Send_8_data[6] = 0x00;
				Send_8_data[7] = 0x02;
				HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
				 HAL_Delay(100);
       }
		}
	}
}

void LCD_ShouYe(void)
{
	if(LCD_Shouye == 1)
	{
		//�������濪�عر�
		LCD_Daochu = 0;
		LCD_Show_Real_time_data_6 = 0;
		LCD_Jiaozhun = 0;
		LCD_Gongcha = 0;
		//��������С���ܿ���
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		Channel_Choose = 0;
		Calibration = 0;
		LCD_Show_Gongcha = 0;			//��ʾ��ʷ���õĹ��� 
	  LCD_Show_JiaoZhun = 0;		//��ʾ��ʷ���õ�У׼
		LCD_USB = 0;

		if(LCD_Show_N_OF_Channels != 0)
		{			
			//�ϴ�LCD_N_OF_Channels
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x22;
			LCD_Data2[0] = (LCD_N_OF_Channels >> 8) & 0xff ;
			LCD_Data2[1] = LCD_N_OF_Channels & 0xff;
			Send_8_data[6] = LCD_Data2[0];
			Send_8_data[7] = LCD_Data2[1];
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
			LCD_Show_N_OF_Channels = 0;
		}
	}
}

void LCD_CeLiang(void)
{
	int i;
	int value;
	int j;
	int k;
	if(LCD_Show_Real_time_data_6 == 1)
	{
		//�������濪�عر�
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Gongcha = 0;
		//��������С���ܿ���
		LCD_Show_Gongcha = 0;			//��ʾ��ʷ���õĹ���  
	  LCD_Show_JiaoZhun = 0;		//��ʾ��ʷ���õ�У׼
		Channel_Choose = 0;
		Calibration = 0;
		LCD_USB = 0;
		for (i =0;i<6;i++)		//��״ͼ��ʾЧ��ȷ��
		{	
			value = (LCD_Upper_GongCha[i] - LCD_Lower_GongCha[i])/5;
			if(LCD_Lower_GongCha[i]<LCD_Real_time_data_6[i] && LCD_Real_time_data_6[i]<(LCD_Lower_GongCha[i]+value))
			{
				LCD_R_G_Gra[i] = 1;
			}
			else if ((LCD_Lower_GongCha[i] + value)<LCD_Real_time_data_6[i] && LCD_Real_time_data_6[i]<(LCD_Lower_GongCha[i]+2*value))
			{
				LCD_R_G_Gra[i] = 2;
			}
			else if ((LCD_Lower_GongCha[i] + 2*value)<LCD_Real_time_data_6[i] && LCD_Real_time_data_6[i]<(LCD_Lower_GongCha[i]+3*value))
			{
				LCD_R_G_Gra[i] = 3;
			}
			else if ((LCD_Lower_GongCha[i] + 3*value)<LCD_Real_time_data_6[i] && LCD_Real_time_data_6[i]<(LCD_Lower_GongCha[i]+4*value))
			{
				LCD_R_G_Gra[i] = 4;
			}
			else if ((LCD_Lower_GongCha[i] + 4*value)<LCD_Real_time_data_6[i] && LCD_Real_time_data_6[i]<LCD_Upper_GongCha[i])
			{
				LCD_R_G_Gra[i] = 5;
			}
			else
			{
				LCD_R_G_Gra[i] = 0;
			}
			//��ʽ���������ݷ����ϴ�
			Send_42_data[2*i + 6] = (LCD_R_G_Gra[i] >> 8)&0XFF;    
			Send_42_data[2*i + 7] = LCD_R_G_Gra[i]&0XFF;
			if(LCD_N_OF_Channels == 2)
			{
				if(LCD_R_G_Gra[0] == 0 || LCD_R_G_Gra[1] == 0)
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_SET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_RESET); 
				}
				else
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_RESET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_SET); 
				}
				if(i>1){
					Send_42_data[2*i + 6] = 0x00;    
					Send_42_data[2*i + 7] = 0x06;
					Send_42_data[18+4*i] = 0x00;
					Send_42_data[19+4*i] = 0x00;                    
					Send_42_data[20+4*i] = 0x00;
					Send_42_data[21+4*i] = 0x00;
				}
				
			}
			else if (LCD_N_OF_Channels == 4)
			{
				if(LCD_R_G_Gra[0] == 0 || LCD_R_G_Gra[1] == 0 || LCD_R_G_Gra[2] == 0 || LCD_R_G_Gra[3] == 0)
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_SET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_RESET); 
				}
				else
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_RESET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_SET); 
				}
				if(i > 3){
					Send_42_data[2*i + 6] = 0x00;    
					Send_42_data[2*i + 7] = 0x06;
					Send_42_data[18+4*i] = 0x00;
					Send_42_data[19+4*i] = 0x00;                    
					Send_42_data[20+4*i] = 0x00;
					Send_42_data[21+4*i] = 0x00;
				}
			}
			else if(LCD_N_OF_Channels == 6)
			{
				if(LCD_R_G_Gra[0] == 0 || LCD_R_G_Gra[1] == 0 || LCD_R_G_Gra[2] == 0 || LCD_R_G_Gra[3] == 0 || LCD_R_G_Gra[4] == 0 || LCD_R_G_Gra[5] == 0)
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_SET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_RESET); 
				}
				else
				{
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_0, GPIO_PIN_RESET); 
					HAL_GPIO_WritePin(GPIOC,GPIO_PIN_2, GPIO_PIN_SET); 
				}
			}
		}
		if(LCD_Show_YuanDu != 0)
		{
			Data_Extremum(LCD_Show_YuanDu);
			Send_10_data[5] =  LCD_Show_YuanDu_Add[1];
			Send_10_data[6] = (LCD_YuanDu>>24)&0xff;
			printf("%x ",Send_10_data[6]);
			Send_10_data[7] =	(LCD_YuanDu>>16)&0xff;
			printf("%x ",Send_10_data[7]);
			Send_10_data[8] =	(LCD_YuanDu>>8)&0xff;
			printf("%x ",Send_10_data[8]);
			Send_10_data[9] =	 LCD_YuanDu&0xff;
			printf("%x \r\n",Send_10_data[9]);
			HAL_UART_Transmit(&huart3,Send_10_data,sizeof(Send_10_data),2000);
			HAL_Delay(50);
		}
		printf("��������\r\n");
		HAL_UART_Transmit(&huart3,Send_42_data,sizeof(Send_42_data),1000);
		HAL_Delay(50);
		if (LCD_DataSave == 1)
		{
			printf("��������\r\n");
			//�ϴ��������ݽ���
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x1e;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
			
			//��������
			Flash_Read_Words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);  //��flash��������
			printf("��������");
			k = Flash_Save_data[30]*6;
			if(k>30||k<0)
			{
				k = 0 ;
				Flash_Save_data[30] = 0;
			}
			printf("k = %d\r\n",k);
				for(j = k;j<k+6;j++)															//************
				{																									//************
					Flash_Save_data[j] = LCD_Real_time_data_6[j-k];	//************
					printf("Flash_Save_data[%d] = %d\r\n",j,Flash_Save_data[j]);
				}																									//************�������ݴ洢��������
				Flash_Save_data[30] = Flash_Save_data[30]+1;			//************
				if (Flash_Save_data[30] == 5)											//************
				{																									//************
					Flash_Save_data[30] = 0;												//************
				}																									//************
			Erase_flash(SAVE_DATA_ADD);												//����flash
			Flash_Write_words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);//�����µ��������flash��
			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢
			
			HAL_Delay(500);
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x1e;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
			LCD_DataSave = 0;
		}
	}
}

void JiaoZhun(void)
{
	if(LCD_Jiaozhun == 1)
	{
		if (LCD_Show_JiaoZhun == 0)
		{
			Send_limit_data[6] = (Flash_Limit_data[0]>>24)&0xff;  
			Send_limit_data[7] = (Flash_Limit_data[0]>>16)&0xff; 	
			Send_limit_data[8] = (Flash_Limit_data[0]>>8)&0xff; 	
			Send_limit_data[9] = Flash_Limit_data[0]&0xff; 
			Send_limit_data[10] = (Flash_Limit_data[3]>>24)&0xff; 
			Send_limit_data[11] =	(Flash_Limit_data[3]>>16)&0xff; 
			Send_limit_data[12] =	(Flash_Limit_data[3]>>8)&0xff; 
			Send_limit_data[13] = Flash_Limit_data[3]&0xff; 
			HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			LCD_Show_JiaoZhun = 1;
		}
		
		uint8_t k;
		//�������濪�عر�
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Gongcha = 0;
		LCD_Show_Real_time_data_6 = 0;
		//��������С���ܿ���
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		LCD_Show_Gongcha = 0;			//��ʾ��ʷ���õĹ���  	
		LCD_USB = 0;
		
		if(Calibration == 1)
		{
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x1f;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			if(Channel_Choose != 0)
			{
			k = Channel_Choose - 1;
			//�õ�У׼���� У׼����ʱ��ʵ�ʵ�ѹ ����ϵ��K
			LCD_Upper_Jiaozhun[Channel_Choose - 1] = usart3_buffer[7]<<24 | usart3_buffer[8]<<16 | usart3_buffer[9]<<8 | usart3_buffer[10];
			LCD_high[Channel_Choose - 1] = ADC_sum_ave[Channel_Choose - 1];
				if(MODE == 1)
				{
					K[Channel_Choose - 1] = ((float)(LCD_Upper_Jiaozhun[Channel_Choose - 1] - LCD_Lower_Jiaozhun[Channel_Choose - 1])/(float)(LCD_high[Channel_Choose - 1] - LCD_low[Channel_Choose - 1]));
				}
				if(MODE == 2)
				{
				K[Channel_Choose - 1] = ((float)(LCD_Upper_Jiaozhun[Channel_Choose - 1] - LCD_Lower_Jiaozhun[Channel_Choose - 1])/(float)(LCD_low[Channel_Choose - 1] - LCD_high[Channel_Choose - 1]));
				}
			//��ȡflash�е�����	
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);  				
			//��������д��Limit������	
			Flash_Limit_data[7*k] = LCD_Upper_Jiaozhun[Channel_Choose - 1];
			Flash_Limit_data[7*k +1] = LCD_high[Channel_Choose - 1];
			Flash_Limit_data[7*k +2] = K[Channel_Choose - 1];
			//��������д��up-down������
//			Flash_Up_Down_data[0] = LCD_Upper_Jiaozhun[Channel_Choose - 1];
			//д��flash	
			Erase_flash(SAVE_LIMIT_ADD);												//����flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//�����µ��������flash��
			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢
				
//			Erase_flash(SAVE_UP_DOWN_ADD);												//����flash 			
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//�����µ��������flash�� 			
//			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢	
				
			
			}
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//�����ݣ��洢������
			Calibration = 0;
		}
		if(Calibration == 2)
		{
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x20;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			if(Channel_Choose != 0)
			{
			k = Channel_Choose - 1;
			LCD_Lower_Jiaozhun[Channel_Choose - 1] = usart3_buffer[7]<<24 | usart3_buffer[8]<<16 | usart3_buffer[9]<<8 | usart3_buffer[10];
			LCD_low[Channel_Choose - 1] = ADC_sum_ave[Channel_Choose - 1];
				if(MODE == 1)
				{
					K[Channel_Choose - 1] = ((float)(LCD_Upper_Jiaozhun[Channel_Choose - 1] - LCD_Lower_Jiaozhun[Channel_Choose - 1])/(float)(LCD_high[Channel_Choose - 1] - LCD_low[Channel_Choose - 1]));
				}
				if(MODE == 2)
				{
				K[Channel_Choose - 1] = ((float)(LCD_Upper_Jiaozhun[Channel_Choose - 1] - LCD_Lower_Jiaozhun[Channel_Choose - 1])/(float)(LCD_low[Channel_Choose - 1] - LCD_high[Channel_Choose - 1]));
				}
				//��flash��������
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);  
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);  			
			//��������д��Limit������	
			Flash_Limit_data[7*k+3] = LCD_Lower_Jiaozhun[Channel_Choose - 1];
			Flash_Limit_data[7*k+4] = LCD_low[Channel_Choose - 1];
			Flash_Limit_data[7*k+2] = K[Channel_Choose - 1];
		//	printf("LCD_Lower_Jiaozhun = %d,LCD_low = %d,K = %f\r\n",LCD_Lower_Jiaozhun[Channel_Choose - 1],LCD_low[Channel_Choose - 1],K[Channel_Choose - 1]);
			//��������д��up-down������	
//			Flash_Up_Down_data[1] = LCD_Lower_Jiaozhun[Channel_Choose - 1];
			//д��flash	
			Erase_flash(SAVE_LIMIT_ADD);												//����flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//�����µ��������flash��
			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢
			//д��flash	
//			Erase_flash(SAVE_UP_DOWN_ADD);												//����flash 			 			
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//�����µ��������flash�� 			 		
//			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢	
			}			
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//�����ݣ��洢������
			Calibration = 0;
		}
	}
}

void GongCha(void)
{
	if(LCD_Gongcha == 1)
	{
		if(LCD_Show_Gongcha == 0)
		{
			Send_limit_data[6] = (Flash_Limit_data[5]>>24)&0xff; 
			Send_limit_data[7] = (Flash_Limit_data[5]>>16)&0xff; 	
			Send_limit_data[8] = (Flash_Limit_data[5]>>8)&0xff; 	
			Send_limit_data[9] = Flash_Limit_data[5]&0xff;
			Send_limit_data[10] = (Flash_Limit_data[6]>>24)&0xff;  
			Send_limit_data[11] =	(Flash_Limit_data[6]>>16)&0xff; 
			Send_limit_data[12] =	(Flash_Limit_data[6]>>8)&0xff; 
			Send_limit_data[13] = Flash_Limit_data[6]&0xff;
			HAL_UART_Transmit(&huart3,Send_limit_data,sizeof(Send_limit_data),1000);
			LCD_Show_Gongcha = 1;
		}
		
		uint8_t k;
		//�������濪�عر�
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Show_Real_time_data_6 = 0;
		//��������С���ܿ���
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0; 
	  LCD_Show_JiaoZhun = 0;		//��ʾ��ʷ���õ�У׼
		LCD_USB = 0;
		if(Calibration == 1)
		{
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x1f;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			
			k = Channel_Choose - 1;
			LCD_Upper_GongCha[Channel_Choose - 1] = usart3_buffer[7]<<24 | usart3_buffer[8]<<16 | usart3_buffer[9]<<8 | usart3_buffer[10];
			//��flash��������
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 			
			//��������д��Limit������	
			Flash_Limit_data[7*k+5] = LCD_Upper_GongCha[Channel_Choose - 1];
//			Flash_Up_Down_data[2] = LCD_Upper_GongCha[Channel_Choose - 1];
			//д��flash	
			Erase_flash(SAVE_LIMIT_ADD);												//����flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//�����µ��������flash��
			HAL_FLASH_Lock();			
			//д��flash	 		
//			Erase_flash(SAVE_UP_DOWN_ADD);													 			 	
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//�����µ��������flash�� 			 		 		
//			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢	
			//�����ݣ��洢������
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//�����ݣ��洢������
			Calibration = 0;
		}
		if(Calibration == 2)
		{
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x20;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			
			k = Channel_Choose - 1;
			LCD_Lower_GongCha[Channel_Choose - 1] = usart3_buffer[7]<<24 | usart3_buffer[8]<<16 | usart3_buffer[9]<<8 | usart3_buffer[10];
			//��flash��������
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 			
			//��������д��Limit������	
			Flash_Limit_data[7*k+6] = LCD_Lower_GongCha[Channel_Choose - 1];
//			Flash_Up_Down_data[3] = LCD_Lower_GongCha[Channel_Choose - 1];
			//д��flash	
			Erase_flash(SAVE_LIMIT_ADD);												//����flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//�����µ��������flash��
			HAL_FLASH_Lock();		
			//д��flash	 		 		
//			Erase_flash(SAVE_UP_DOWN_ADD);													 	
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//�����µ��������flash�� 			 		
//			HAL_FLASH_Lock();																											//����flash  ��������ݵĴ洢				
			//�����ݣ��洢������

			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//�����ݣ��洢������
			Calibration = 0;
		}
	}
}

void DaoChu(void)
{
	if(LCD_Daochu == 1)
	{
		int i;
		int k;
		//�������濪�عر�
		LCD_Gongcha  = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Show_Real_time_data_6 = 0;
		//��������С���ܿ���
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		Channel_Choose = 0;
		Calibration = 0;
		//��ʾ�洢����
		if(LCD_SaveDataShow == 1)
		{
			Flash_Read_Words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);//��ȡ�洢������
			for(i = 0;i<31;i++)
			{
				printf("Flash_Save_data[%d] = %d ",i,Flash_Save_data[i]);
			}
			for(i = 0;i<30;i++)
			{
				Send_126_data[4*i+6] = (Flash_Save_data[i]>>24)&0xff ;
				Send_126_data[4*i+7] = (Flash_Save_data[i]>>16)&0xff ;                    
				Send_126_data[4*i+8] = (Flash_Save_data[i]>>8)&0xff ;
				Send_126_data[4*i+9] = (Flash_Save_data[i])&0xff ;
			}
			k = Flash_Save_data[30];
			if(k == 0)
			{
				k = 5;
			}
			Send_126_data[126] =  (k>>24)&0xff ;
			Send_126_data[127] = (k>>16)&0xff ;                    
			Send_126_data[128] = (k>>8)&0xff ;
			Send_126_data[129] = k&0xff ;
			HAL_UART_Transmit(&huart3,Send_126_data,sizeof(Send_126_data),1000);
			LCD_SaveDataShow = 0;
			LCD_USB = 1;
		}
	}
}

