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
	
	uint8_t Send_8_data[10];				//首页显示选择几个通道
	uint8_t MODE;									//测量模式选择
	uint8_t Send_10_data[12];			//测量界面的圆度值
	uint8_t Send_42_data[44];			//测量界面显示实时示数与动态图
	uint8_t Send_126_data[132];		//数据导出界面显示的历史数据
	uint8_t Send_limit_data[16];	//校准和公差设置的上下限值
	uint8_t LCD_Show_Gongcha;			//显示历史设置的公差
	uint8_t LCD_Show_JiaoZhun;		//显示历史设置的校准
	uint8_t LCD_USB;							//USB导出标志位
	uint8_t MODE_CASE[] = {0x5A,0XA5,0X07,0X82,0X00,0X84,0X5A,0X01,0X00,0X00};			//界面选择数组
	
	
	int Flash_Save_data[31];	//flash存储历史数据
	int Flash_Limit_data[42];	//flash存储的六路校准以及公差数据 储存顺序为 （上限校准 上限电压 K值 下限校准 下限电压 上限公差 下限公差）
	int Flash_Up_Down_data[4];	//flash存储的最后一组设置的校准值及数据
	
	#define SAVE_DATA_ADD          0x08010000
	#define SAVE_LIMIT_ADD  			 0x08010800
	#define SAVE_UP_DOWN_ADD			 0x08011000
	
	FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */
	FIL MyFile;                   /* File object */
	
//	Calibration = 0;

//************************求圆度***************************	
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
	printf("最大值为%d\r\n",LCD_Max);
	printf("最小值为%d\r\n",LCD_Min);
	printf("圆度值为%d\r\n",LCD_YuanDu);
}
//***************************获得需要显示的数值*********************************
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

//****************************flash打开擦除**************************
void Erase_flash(uint32_t Address)
{
	FLASH_EraseInitTypeDef LIMIT_FLASH;             //定义需要擦除的信息结构体
	HAL_FLASH_Unlock();
	LIMIT_FLASH.TypeErase = FLASH_TYPEERASE_PAGES;  //标明flash执行页面只做擦除操作
	LIMIT_FLASH.PageAddress = Address;							//声明要擦除的地址
	LIMIT_FLASH.NbPages =1;													//声明要擦除的页数

	uint32_t PageError =0;
	HAL_FLASHEx_Erase(&LIMIT_FLASH,&PageError);//调用擦除函数擦除
}
//****************************把一段数写入flash***************************
void Flash_Write_words (uint32_t WriteAddr,uint32_t *pBuffer,uint8_t NumToWrite)
{
	uint8_t i;
	for(i = 0;i < NumToWrite;i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,WriteAddr,pBuffer[i]);
		WriteAddr+=4;
	}
}
//**************************读取从flash中读取单个数*****************************
uint32_t read_flash(uint32_t Address)  
{
	uint32_t pValue= *(__IO uint32_t*)(Address);
	return pValue;
}
//***************************把一段数从flash中读取*****************************
void Flash_Read_Words(uint32_t ReadAddr,uint32_t *pBuffer,uint8_t NumToRead)  
{
	uint8_t i;
	for(i=0;i<NumToRead;i++)
	{
		pBuffer[i]=read_flash(ReadAddr);//读取1个32位数据.
		ReadAddr+=4;//偏移1个地址.	
	}		
}
//******************************发送数据初始化*******************************
void Send_Data_Iint(void)
{
		uint8_t i;
		//首页的显示选择了几个通道
		Send_8_data[0] = LCD_INFO[0];
		Send_8_data[1] = LCD_INFO[1];
		Send_8_data[2] = 0x05;
		Send_8_data[3] = 0x82;
		Send_8_data[8] = 0x0D;
		Send_8_data[9] = 0x0A;
		//测量界面的动态图与实时示数
		Send_42_data[0] = LCD_INFO[0];
		Send_42_data[1] = LCD_INFO[1];
		Send_42_data[2] = 0x27;
		Send_42_data[3] = 0x82;
		Send_42_data[4] = 0x11;
		Send_42_data[5] = 0x00;
		Send_42_data[42] = 0x0D;
		Send_42_data[43] = 0x0A;
		//测量界面的圆度值
		Send_10_data[0] =  LCD_INFO[0];
		Send_10_data[1] =  LCD_INFO[1];
		Send_10_data[2] =  0x07;
		Send_10_data[3] =  0x82;
		Send_10_data[4] =  0X11;
		Send_10_data[10] =  0x0D;
		Send_10_data[11] =  0x0A;
		//数据导出界面的历史数据值
		Send_126_data[0] = LCD_INFO[0];
		Send_126_data[1] = LCD_INFO[1];
		Send_126_data[2] = 0x7F;
		Send_126_data[3] = 0x82;
		Send_126_data[4] = 0x11;
		Send_126_data[5] = 0x30;
		Send_126_data[130] = 0x0D;
		Send_126_data[131] = 0x0A;
		//校准和公差界面的上下限值
		Send_limit_data[0] = LCD_INFO[0];
		Send_limit_data[1] = LCD_INFO[1];
		Send_limit_data[2] = 0x0b;
		Send_limit_data[3] = 0x82;
		Send_limit_data[4] = 0x10;
		Send_limit_data[5] = 0x22;
		Send_limit_data[14] = 0x0D;
		Send_limit_data[15] = 0x0A;

		//将存储的上下限数据读出来
		Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
		for(i = 0;i<6;i++)
		{
		 LCD_Upper_Jiaozhun[i] =  Flash_Limit_data[7*i];    //校准最大值
		 LCD_high[i]           =  Flash_Limit_data[7*i+1];	//校准最大值时的电压值
		 K[i]                  =  Flash_Limit_data[7*i+2];
		 LCD_Lower_Jiaozhun[i] =  Flash_Limit_data[7*i+3];	//校准最小值
		 LCD_low[i]            =  Flash_Limit_data[7*i+4];	//校准最小时的电压值
		 LCD_Upper_GongCha[i]  =  Flash_Limit_data[7*i+5];	//公差最大值
		 LCD_Lower_GongCha[i]  =  Flash_Limit_data[7*i+6];	//公差最小值
		}
		Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 
		for(i = 0;i<7;i++)
		{
			printf("Flash_Limit_data[%d] = %d\r\n",i,Flash_Limit_data[i]);
		}
		//黄灯电平置高
		HAL_GPIO_WritePin(GPIOC,GPIO_PIN_1, GPIO_PIN_SET); 
		//485芯片初始值置低
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
//触摸检测
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
		//**************************************首页********************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x00)	//进入首页
		{
			printf("____进入首页_______\r\n");
			LCD_Shouye = 1;
			LCD_Daochu = 0;
			LCD_Show_YuanDu = 0;
			LCD_Show_Real_time_data_6 = 0;
			LCD_DataSave = 0;
			LCD_Jiaozhun = 0;
			LCD_Gongcha = 0;
			Channel_Choose = 0;
			Calibration = 0;
			
			//选择显示界面
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
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x70)	//内径测量
		{
			MODE = 1;
		}
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x72)	//外径测量
		{
			MODE = 2;
		}
		if(usart3_buffer[4] == 0x11 && usart3_buffer[5] == 0x74)	//显示电压
		{
			MODE = 3;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x26)	//选择两个通道
		{
			printf("____选择两个通道______\r\n");
			LCD_N_OF_Channels = 2;
			LCD_Show_N_OF_Channels = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x28)	//选择四个通道
		{
			printf("____选择四个通道______\r\n");
			LCD_N_OF_Channels = 4;
			LCD_Show_N_OF_Channels = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x2a)	//选择六个通道
		{
			printf("____选择六个通道______\r\n");
			LCD_N_OF_Channels = 6;
			LCD_Show_N_OF_Channels = 1;
		}
		//**********************************测量***********************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x02)  //进入测量界面
		{
			printf("____进入测量界面_____\r\n");
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
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0a)	//显示通道一圆度
		{
			printf("______ 显示通道一圆度 ______\r\n");
			LCD_Show_YuanDu = 1;
			LCD_Show_YuanDu_Add[1]= 0x12;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0c)	//显示通道二圆度
		{
			printf("______ 显示通道二圆度\r\n");
			LCD_Show_YuanDu = 2;
			LCD_Show_YuanDu_Add[1]= 0x14;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x0e)	//显示通道三圆度
		{
			printf("______ 显示通道三圆度\r\n");
			LCD_Show_YuanDu = 3;
			LCD_Show_YuanDu_Add[1]= 0x16;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x10)	//显示通道四圆度
		{
			printf("______ 显示通道四圆度\r\n");
			LCD_Show_YuanDu = 4;
			LCD_Show_YuanDu_Add[1]= 0x18;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x12)	//显示通道五圆度
		{
			printf("______ 显示通道五圆度\r\n");
			LCD_Show_YuanDu = 5;
			LCD_Show_YuanDu_Add[1]= 0x1a;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x14)	//显示通道六圆度
		{
			printf("______ 显示通道六圆度\r\n");
			LCD_Show_YuanDu = 6;
			LCD_Show_YuanDu_Add[1]= 0x1c;
			LCD_Max = 0;
			LCD_Min = 1000000;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x2c)	//保存此组数据
		{
			printf("______ 保存此组数据\r\n");
			LCD_DataSave = 1;
		}
		//*********************************校准************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x04)	//进入校准界面
		{
			printf("______ 进入校准界面\r\n");
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
		//*******************************公差***************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x06)
		{
			printf("______ 进入公差界面\r\n");
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
		//*********************************校准通道设置****************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x16)
		{
			printf("______ 选择通道一进行设置\r\n");
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
			printf("______ 选择通道二进行设置\r\n");
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
			printf("______ 选择通道三进行设置\r\n");
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
			printf("______ 选择通道四进行设置\r\n");
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
			printf("______ 选择通道五进行设置\r\n");
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
			printf("______ 选择通道六进行设置\r\n");
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
		//********************************校准按钮*************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x22)
		{
			printf("______ 进行上限校准\r\n");
			int opp;
			for (opp = 0;opp<11;opp++)
			{
			printf(" %x",usart3_buffer[opp]);
			}
			Calibration = 1;
		}
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x24)
		{
			printf("______ 进行下限校准\r\n");
			Calibration = 2;
			int opp;
			for (opp = 0;opp<11;opp++)
			{
			printf(" %x",usart3_buffer[opp]);
			}
		}
		//*******************************数据导出*****************************************
		if(usart3_buffer[4] == 0x10 && usart3_buffer[5] == 0x08)
		{
			printf("______ 数据导出\r\n");
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
//***************************************优盘导出**************************************************
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
			f_printf(&MyFile,"%s ","第一组数据");
			for (i = 0;i<5;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[5]);
			f_printf(&MyFile,"%s ","第二组数据");
			for (i = 6;i<11;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[11]);
			f_printf(&MyFile,"%s ","第三组数据");
			for (i = 12;i<17;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[17]);
			f_printf(&MyFile,"%s ","第四组数据");
			for (i = 18;i<23;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[23]);
			f_printf(&MyFile,"%s ","第五组数据");
			for (i = 24;i<29;i++)
			{
			f_printf(&MyFile,"%d ",Flash_Save_data[i]);
			}
			f_printf(&MyFile,"%d \r\n",Flash_Save_data[29]);
			printf("写入文件\r\n");
			res = f_lseek(&MyFile, 0); 
			if(res != FR_OK)
      {
         /* 'STM32.TXT' file Write or EOF Error */
        Error_Handler();
				printf("数据导出错误\r\n");
      }
      else
       {
          /* Close the open text file */
				 printf("关闭优盘\r\n");
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
		//其他界面开关关闭
		LCD_Daochu = 0;
		LCD_Show_Real_time_data_6 = 0;
		LCD_Jiaozhun = 0;
		LCD_Gongcha = 0;
		//其他界面小功能开关
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		Channel_Choose = 0;
		Calibration = 0;
		LCD_Show_Gongcha = 0;			//显示历史设置的公差 
	  LCD_Show_JiaoZhun = 0;		//显示历史设置的校准
		LCD_USB = 0;

		if(LCD_Show_N_OF_Channels != 0)
		{			
			//上传LCD_N_OF_Channels
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
		//其他界面开关关闭
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Gongcha = 0;
		//其他界面小功能开关
		LCD_Show_Gongcha = 0;			//显示历史设置的公差  
	  LCD_Show_JiaoZhun = 0;		//显示历史设置的校准
		Channel_Choose = 0;
		Calibration = 0;
		LCD_USB = 0;
		for (i =0;i<6;i++)		//柱状图显示效果确定
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
			//格式化存数数据方便上传
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
		printf("发送数据\r\n");
		HAL_UART_Transmit(&huart3,Send_42_data,sizeof(Send_42_data),1000);
		HAL_Delay(50);
		if (LCD_DataSave == 1)
		{
			printf("保存数据\r\n");
			//上传保存数据进度
			Send_8_data[4] = 0x11;
			Send_8_data[5] = 0x1e;
			Send_8_data[6] = 0x00;
			Send_8_data[7] = 0x01;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),1000);
			
			//储存数据
			Flash_Read_Words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);  //从flash读出数据
			printf("读出数据");
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
				}																									//************把新数据存储到数组中
				Flash_Save_data[30] = Flash_Save_data[30]+1;			//************
				if (Flash_Save_data[30] == 5)											//************
				{																									//************
					Flash_Save_data[30] = 0;												//************
				}																									//************
			Erase_flash(SAVE_DATA_ADD);												//擦除flash
			Flash_Write_words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);//将更新的数组存入flash中
			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储
			
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
		//其他界面开关关闭
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Gongcha = 0;
		LCD_Show_Real_time_data_6 = 0;
		//其他界面小功能开关
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		LCD_Show_Gongcha = 0;			//显示历史设置的公差  	
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
			//得到校准上限 校准上限时的实际电压 比例系数K
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
			//读取flash中的数据	
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);  				
			//将新数据写入Limit数组中	
			Flash_Limit_data[7*k] = LCD_Upper_Jiaozhun[Channel_Choose - 1];
			Flash_Limit_data[7*k +1] = LCD_high[Channel_Choose - 1];
			Flash_Limit_data[7*k +2] = K[Channel_Choose - 1];
			//将新数据写入up-down数组中
//			Flash_Up_Down_data[0] = LCD_Upper_Jiaozhun[Channel_Choose - 1];
			//写入flash	
			Erase_flash(SAVE_LIMIT_ADD);												//擦除flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//将更新的数组存入flash中
			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储
				
//			Erase_flash(SAVE_UP_DOWN_ADD);												//擦除flash 			
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//将更新的数组存入flash中 			
//			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储	
				
			
			}
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//读数据，存储到上限
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
				//从flash读出数据
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);  
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);  			
			//将新数据写入Limit数组中	
			Flash_Limit_data[7*k+3] = LCD_Lower_Jiaozhun[Channel_Choose - 1];
			Flash_Limit_data[7*k+4] = LCD_low[Channel_Choose - 1];
			Flash_Limit_data[7*k+2] = K[Channel_Choose - 1];
		//	printf("LCD_Lower_Jiaozhun = %d,LCD_low = %d,K = %f\r\n",LCD_Lower_Jiaozhun[Channel_Choose - 1],LCD_low[Channel_Choose - 1],K[Channel_Choose - 1]);
			//将新数据写入up-down数组中	
//			Flash_Up_Down_data[1] = LCD_Lower_Jiaozhun[Channel_Choose - 1];
			//写入flash	
			Erase_flash(SAVE_LIMIT_ADD);												//擦除flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//将更新的数组存入flash中
			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储
			//写入flash	
//			Erase_flash(SAVE_UP_DOWN_ADD);												//擦除flash 			 			
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//将更新的数组存入flash中 			 		
//			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储	
			}			
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//读数据，存储到下限
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
		//其他界面开关关闭
		LCD_Daochu = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Show_Real_time_data_6 = 0;
		//其他界面小功能开关
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0; 
	  LCD_Show_JiaoZhun = 0;		//显示历史设置的校准
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
			//从flash读出数据
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 			
			//将新数据写入Limit数组中	
			Flash_Limit_data[7*k+5] = LCD_Upper_GongCha[Channel_Choose - 1];
//			Flash_Up_Down_data[2] = LCD_Upper_GongCha[Channel_Choose - 1];
			//写入flash	
			Erase_flash(SAVE_LIMIT_ADD);												//擦除flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//将更新的数组存入flash中
			HAL_FLASH_Lock();			
			//写入flash	 		
//			Erase_flash(SAVE_UP_DOWN_ADD);													 			 	
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//将更新的数组存入flash中 			 		 		
//			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储	
			//读数据，存储到上限
			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//读数据，存储到上限
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
			//从flash读出数据
			Flash_Read_Words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);
//			Flash_Read_Words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4); 			
			//将新数据写入Limit数组中	
			Flash_Limit_data[7*k+6] = LCD_Lower_GongCha[Channel_Choose - 1];
//			Flash_Up_Down_data[3] = LCD_Lower_GongCha[Channel_Choose - 1];
			//写入flash	
			Erase_flash(SAVE_LIMIT_ADD);												//擦除flash
			Flash_Write_words(SAVE_LIMIT_ADD,Flash_Limit_data,sizeof(Flash_Limit_data)/4);//将更新的数组存入flash中
			HAL_FLASH_Lock();		
			//写入flash	 		 		
//			Erase_flash(SAVE_UP_DOWN_ADD);													 	
//			Flash_Write_words(SAVE_UP_DOWN_ADD,Flash_Up_Down_data,sizeof(Flash_Up_Down_data)/4);//将更新的数组存入flash中 			 		
//			HAL_FLASH_Lock();																											//所著flash  完成新数据的存储				
			//读数据，存储到下限

			HAL_Delay(500);
			Send_8_data[7] = 0x02;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			HAL_Delay(500);
			Send_8_data[7] = 0x00;
			HAL_UART_Transmit(&huart3,Send_8_data,sizeof(Send_8_data),100);
			//读数据，存储到下限
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
		//其他界面开关关闭
		LCD_Gongcha  = 0;
		LCD_Shouye = 0;
		LCD_Jiaozhun = 0;
		LCD_Show_Real_time_data_6 = 0;
		//其他界面小功能开关
		LCD_Show_YuanDu = 0;
		LCD_DataSave = 0;
		Channel_Choose = 0;
		Calibration = 0;
		//显示存储数据
		if(LCD_SaveDataShow == 1)
		{
			Flash_Read_Words(SAVE_DATA_ADD,Flash_Save_data,sizeof(Flash_Save_data)/4);//读取存储的数据
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

