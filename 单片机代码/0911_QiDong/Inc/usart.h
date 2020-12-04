/**
  ******************************************************************************
  * File Name          : USART.h
  * Description        : This file provides code for the configuration
  *                      of the USART instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __usart_H
#define __usart_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

/* USER CODE BEGIN Private defines */
	 
extern uint8_t usart3_buffer[11];;
extern uint8_t USART3_BUFF[11];
extern uint8_t res;				//����������
	 
extern int LCD_Upper_Jiaozhun[6];
extern int LCD_Lower_Jiaozhun[6];
extern int LCD_Upper_GongCha[6];
extern int LCD_Lower_GongCha[6];
extern int K[6];
extern int LCD_high[6];
extern int LCD_low[6];
	
extern uint8_t LCD_Need_check;									//��⵽����	
extern uint8_t LCD_N_OF_Channels;				//��Ҫ������ͨ������
extern uint8_t LCD_Show_N_OF_Channels;		//��Ļ��ʾ������ͨ����������
extern uint8_t LCD_Show_Real_time_data_6;//��Ļ��ʾ����ͨ��ʵ��ֵ�Ŀ���
extern uint8_t LCD_Show_YuanDu;						//��Ļ��ʾ����ͨ��Բ�ȵĿ���
extern uint8_t LCD_DataSave;							//����������ݵĿ���
extern uint8_t LCD_Shouye;								//������ҳ�ı�־λ
extern uint8_t LCD_Jiaozhun;							//����У׼�ı�־λ
extern uint8_t LCD_Gongcha;							//���빫��ı�־λ
extern uint8_t Channel_Choose;						//ѡ��У׼ͨ��
extern uint8_t Calibration;							//У׼��ťѡ��
extern uint8_t LCD_Daochu;										//�������ݵ�������ı�־λ
extern uint8_t LCD_SaveDataShow;					//��ʾ�������ݵĿ���
extern uint8_t LCD_Show_YuanDu_Add[2];
/* USER CODE END Private defines */

void MX_UART4_Init(void);
void MX_UART5_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ usart_H */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
