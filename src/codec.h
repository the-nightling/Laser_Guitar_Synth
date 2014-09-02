//*************************************
//
//  header for codec related functions
//
//*************************************

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_i2c.h"
#include "stm32f4xx_spi.h"

#ifndef __CODEC_H
#define __CODEC_H

#define ALTFUNCSTATUS 	3  //11

//pins to codec
#define I2S3_WS_PIN 	GPIO_Pin_4   //port A

#define I2S3_MCLK_PIN 	GPIO_Pin_7   //port C
#define I2S3_SCLK_PIN 	GPIO_Pin_10  //port C
#define I2S3_SD_PIN 	GPIO_Pin_12  //port C

#define CODEC_RESET_PIN GPIO_Pin_4  //port D

#define I2C_SCL_PIN		GPIO_Pin_6  //port B
#define I2C_SDA_PIN		GPIO_Pin_9  //port B

#define CODEC_I2C I2C1
#define CODEC_I2S SPI3

#define CORE_I2C_ADDRESS 0x33
#define CODEC_I2C_ADDRESS 0x94

#define CODEC_MAPBYTE_INC 0x80

//register map bytes for CS42L22 (see page 35)
#define CODEC_MAP_PWR_CTRL1 0x02
#define CODEC_MAP_PWR_CTRL2 0x04
#define CODEC_MAP_CLK_CTRL  0x05
#define CODEC_MAP_IF_CTRL1  0x06
#define CODEC_MAP_PLAYBACK_CTRL1 0x0D

//function prototypes
void codec_init();
void codec_ctrl_init();
void send_codec_ctrl(uint8_t controlBytes[], uint8_t numBytes);
uint8_t read_codec_register(uint8_t mapByte);


#endif /* __CODEC_H */
