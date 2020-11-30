#ifndef __Function_interface_H
#define __Function_interface_H

#include "main.h"
#include "adc.h"
#include "dma.h"
#include "fatfs.h"
#include "usart.h"
#include "usb_host.h"
#include "gpio.h"

extern int LCD_Max;
extern int LCD_Min;
extern int LCD_YuanDu;
extern uint8_t Send_42_data[44];
extern uint8_t Send_8_data[10];		
extern uint8_t LCD_USB;							//USB导出标志位
extern void Touch_check(void);
extern void Data_Extremum(uint8_t number);
extern void MSC_Application(void);
extern void LCD_ShouYe(void);
extern void LCD_CeLiang(void);
extern void Get_Real_Time_data(void);
extern void Send_Data_Iint(void);
extern void JiaoZhun(void);
extern void GongCha(void);
extern void DaoChu(void);


#endif

