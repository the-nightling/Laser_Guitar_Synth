/**
 *****************************************************************************
 **
 ** EEE3074W Project
 ** Laser Guitar Synthesizer
 ** Team 10
 **
 *****************************************************************************
 */

/* Includes */
#include "stm32f4xx.h"
#include "stm32f4_discovery.h"
#include "codec.h"
#include <math.h>

/* Private Macros */
#define OUTBUFFERSIZE 44100
#define DACBUFFERSIZE 600

/* Private Global Variables */
__IO uint16_t IC1Value = 0;			// Stores length of beam break pulse
__IO uint8_t string_plucked = 0;	// timer flag
__IO uint16_t ADC1_val;				// for volume control

volatile uint8_t outBuffer[OUTBUFFERSIZE];
volatile uint8_t counter = 0;
volatile uint8_t electrify = 0;		// electric mode
volatile uint8_t mux_enable = 1;
volatile uint32_t duration = 44100;	// controls duration of note
volatile float amplitude = 1.0;		// controls volume via duration of pluck
volatile float volume = 0.5;		// controls volume via volume knob

volatile uint8_t stringNo = 6;
volatile uint8_t octave = 4;
volatile float noteFreq = 20.6;

// Guitar notes reference
// C = 16.35	          C3 = 338, C4 = 168
// C#= 17.32	          C#3= 318, C#4= 160
// D = 18.35	          D3 = 300, D4 = 150
// Eb= 19.45	          Eb3= 284, Eb4= 142
// E = 20.60	E2 = 536, E3 = 268, E4 = 134
// F = 21.83	F2 = 506, F3 = 252, F4 = 126
// F#= 23.12	F#2= 476, F#3= 238, F#4= 120
// G = 24.50	G2 = 450, G3 = 226, G4 = 112
// G#= 25.96	G#2= 424, G#3= 212, G#4= 106
// A = 27.50	A2 = 400, A3 = 200
// Bb= 29.14	Bb2= 378, Bb3= 190
// B = 30.87	B2 = 358, B3 = 178

//				Free		Fret1		Fret2		Fret3		Fret4
// String 6:	E2 = 536	F2 = 506	F#2= 476	G2 = 450	G#2= 424
// String 5:	A2 = 400	Bb2= 378	B2 = 358	C3 = 338	C#3= 318
// String 4:	D3 = 300	Eb3= 284	E3 = 268	F3 = 252	F#3= 238
// String 3:	G3 = 226	G#3= 212	A3 = 200	Bb3= 190	B3 = 178
// String 2:	B3 = 178	C4 = 168	C#4= 160	D4 = 150	Eb4= 142
// String 1:	E4 = 134	F4 = 126	F#4= 120	G4 = 112	G#4= 106

/* Private Function Prototypes */
void RCC_Configuration(void);
void GPIO_Configuration(void);
void Timer_Configuration(void);
void NVIC_Configuration(void);
void RNG_Configuration(void);
void ADC_Configuration(void);

/* IRQ Handlers */
/*
 * Cycle through 6 laser strings via multiplexer
 */
void TIM2_IRQHandler(void)
{
	// Clear TIM2 interrupt pending bit
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		if (mux_enable == 1) {

			// set multiplexer input select pins
			if(counter == 0)
			{
				GPIOE->BSRRH = GPIO_Pin_7;
				GPIOE->BSRRH = GPIO_Pin_9;
				GPIOE->BSRRH = GPIO_Pin_11;
			}
			else if (counter == 1)
			{
				GPIOE->BSRRL = GPIO_Pin_7;
				GPIOE->BSRRH = GPIO_Pin_9;
				GPIOE->BSRRH = GPIO_Pin_11;
			}
			else if (counter == 2)
			{
				GPIOE->BSRRH = GPIO_Pin_7;
				GPIOE->BSRRL = GPIO_Pin_9;
				GPIOE->BSRRH = GPIO_Pin_11;
			}
			else if (counter == 3)
			{
				GPIOE->BSRRL = GPIO_Pin_7;
				GPIOE->BSRRL = GPIO_Pin_9;
				GPIOE->BSRRH = GPIO_Pin_11;
			}
			else if (counter == 4)
			{
				GPIOE->BSRRH = GPIO_Pin_7;
				GPIOE->BSRRH = GPIO_Pin_9;
				GPIOE->BSRRL = GPIO_Pin_11;
			}
			else if (counter == 5)
			{
				GPIOE->BSRRL = GPIO_Pin_7;
				GPIOE->BSRRH = GPIO_Pin_9;
				GPIOE->BSRRL = GPIO_Pin_11;
			}

			counter++;
			if(counter > 5)
			{
				counter = 0;
			}
		}
	}
}

/*
 * Trigger if laser string is plucked
 */
void TIM5_IRQHandler(void)
{
	// Clear TIM5 Capture compare interrupt pending bit (falling edge)
	if(TIM_GetITStatus(TIM5, TIM_IT_CC1) == SET) {
		TIM_ClearITPendingBit(TIM5, TIM_IT_CC1);
		mux_enable = 0;

		// Get the Input Capture value
		//IC1Value = TIM_GetCapture1(TIM5);
		volume = 10*((float)ADC1_val/59456);

		//amplitude = (float)(volume)*(1-(float)IC1Value/0xFFFFFFFF);
		amplitude = (float)(volume);

		// Let us know that the string was plucked
		string_plucked = 1;
	}

	// Clear TIM5 Capture compare interrupt pending bit (rising edge)
	if(TIM_GetITStatus(TIM5, TIM_IT_CC2) == SET) {
		TIM_ClearITPendingBit(TIM5, TIM_IT_CC2);
		mux_enable = 1;
	}
}

/*
 * Electric mode switch stuff
 */
void EXTI1_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line1) != RESET)
	{
		if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == SET)
		{
			electrify = 1;
		}
		else
		{
			electrify = 0;
		}

		/* Clear the EXTI line 1 pending bit */
		EXTI_ClearITPendingBit(EXTI_Line1);
	}
}

/**
 **===========================================================================
 **
 **  Abstract: main program
 **
 **===========================================================================
 */
int main(void)
{
	volatile uint32_t sampleCounter = 0;
	volatile int16_t sample = 0;
	volatile uint8_t DACBuffer[DACBUFFERSIZE];
	volatile uint8_t tempBuffer[DACBUFFERSIZE];
	volatile uint8_t noiseBuffer[DACBUFFERSIZE];

	// duration/(note frequency x 2^octave) if odd, add 1
	uint16_t DACBufferSize = (uint16_t)(((float)44100/(noteFreq*pow(2, octave))));
	if(DACBufferSize & 0x00000001)
		DACBufferSize +=1;

	SystemInit();
	RCC_Configuration();
	GPIO_Configuration();
	Timer_Configuration();
	NVIC_Configuration();
	RNG_Configuration();
	codec_init();
	codec_ctrl_init();
	ADC_Configuration();

	// Fill buffer with white noise
	uint16_t n, m;
	uint32_t random = 0;
	for (n = 0; n<DACBUFFERSIZE; n++)
	{
		while(RNG_GetFlagStatus(RNG_FLAG_DRDY) == 0);
		random = RNG_GetRandomNumber();
		noiseBuffer[n] = (uint8_t)(((0xFF+1)/2)*(2*(((float)random)/0xFFFFFFFF)));
		DACBuffer[n] = noiseBuffer[n];
		RNG_ClearFlag(RNG_FLAG_DRDY);
	}

	// infinite loop
	while(1)
	{
		if(string_plucked == 1)
		{
			// reset timer flag
			string_plucked = 0;

			// set note based on laser string plucked
			if(counter == 5)		// D3(3)
			{
				noteFreq = 24.5;
				octave = 3;
			}
			else if(counter == 4)	// D2(4)
			{
				noteFreq = 18.35;
				octave = 3;
			}
			else if(counter == 3)	// D1(5)
			{
				noteFreq = 27.5;
				octave = 2;
			}
			else if(counter == 2)	// D0(6)
			{
				noteFreq = 20.6;
				octave = 2;
			}
			else if(counter == 1)	// D5(2)
			{
				noteFreq = 30.87;
				octave = 3;
			}
			else if(counter == 0)	// D4(1)
			{
				noteFreq = 20.6;
				octave = 4;
			}

			// update note
			DACBufferSize = (uint16_t)(((float)44100/(noteFreq*pow(2, octave))));
			if(DACBufferSize & 0x00000001)
				DACBufferSize +=1;

			// Synthesize note
			volatile uint16_t i;
			volatile uint16_t j = 0;
			for(i = 0; i < duration; i++)
			{
				if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE))
				{
					SPI_I2S_SendData(CODEC_I2S, 0);
				}

				// karplus-strong algorithm
				if(j != DACBufferSize-1)
				{
					tempBuffer[j] = (uint8_t)(((DACBuffer[j]+DACBuffer[j+1])/2.0)*0.999);
				}
				else
				{
					tempBuffer[j] = (uint8_t)(((DACBuffer[j]+tempBuffer[0])/2.0)*0.999);
				}
				outBuffer[i] = (uint8_t)tempBuffer[j];

				// electric mode
				if(electrify == 1 && outBuffer[i] >= 105)
				{
					//outBuffer[i] = 55;
				}
				else if(electrify == 1 && outBuffer[i] < 105)
				{
					outBuffer[i] = 180;
				}

				j++;

				if(j == DACBufferSize)
				{
					for(n=0; n < DACBufferSize; n++)
					{
						DACBuffer[n] = tempBuffer[n];

						if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE))
						{
							SPI_I2S_SendData(CODEC_I2S, 0);
						}
					}
					j = 0;
				}

			}
			n = 1000;

			// output sound
			while(1)
			{
				if(string_plucked == 1) {
					break;
				}

				// output sound
				if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE))
				{
					SPI_I2S_SendData(CODEC_I2S, sample);

					//only update on every second sample to ensure that L & R channels have the same sample value
					if (sampleCounter & 0x00000001)
					{
						if(n >= duration)
						{
							n = 0;
							mux_enable = 1;
							break;	// exit loop when note ends
						}

						sample = (int16_t)(amplitude*outBuffer[n]);

						// change increment value to change frequency
						n += 1;
					}
					sampleCounter++;
				}

				if (sampleCounter == 96000)
				{
					sampleCounter = 0;
				}
			}

			// fill buffer with white noise again
			for(m=0; m < DACBufferSize; m++)
			{
				DACBuffer[m] = noiseBuffer[m];

				if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE))
				{
					SPI_I2S_SendData(CODEC_I2S, 0);
				}
			}
		}

		// while laser not broken
		if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE))
		{
			SPI_I2S_SendData(CODEC_I2S, 0);
		}
	}
}



void RCC_Configuration(void)
{
	// clocks for GPIOA, GPIOB, GPIOC and GPIOD have already been enable in codec.c
	// enable clock for GPIOA, GPIOB, GPIOE and DMA2
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA|RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_DMA2, ENABLE);

	// enable clock for Random Number Generator
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);

	// enable clock for timers 2 and 5
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5|RCC_APB1Periph_TIM2, ENABLE);

	// enable clock for SYSCFG for EXTI
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG|RCC_APB2Periph_ADC1, ENABLE);
}

void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* TIM5 channel2 configuration : PA.01 */
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Connect TIM pin to AF2 */
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_TIM5);

	/* Electric mode switch configuration: PB.01 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Connect EXTI Line to GPIO Pin */
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource1);

	EXTI_InitTypeDef EXTI_InitStructure;

	/* Configure EXTI line */
	EXTI_InitStructure.EXTI_Line = EXTI_Line1;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* Volume pin configuration */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Multiplexer switching configuration */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

}

void NVIC_Configuration(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	/* Enable the TIM5 global Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);

	/* Enable and set Button EXTI Interrupt to the lowest priority */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);

	/* Enable the TIM2 global Interrupt*/
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);
}

void Timer_Configuration(void)
{
	TIM_ICInitTypeDef  TIM_ICInitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	TIM_TimeBaseStructure.TIM_Period = 0xFFFFFFFF;
	TIM_TimeBaseStructure.TIM_Prescaler = 2000;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x3;

	TIM_PWMIConfig(TIM5, &TIM_ICInitStructure);

	/* Select the TIM5 Input Trigger: TI2FP2 */
	TIM_SelectInputTrigger(TIM5, TIM_TS_TI2FP2);

	/* Select the slave Mode: Reset Mode */
	TIM_SelectSlaveMode(TIM5, TIM_SlaveMode_Reset);
	TIM_SelectMasterSlaveMode(TIM5,TIM_MasterSlaveMode_Enable);

	/* TIM enable counter */
	TIM_Cmd(TIM5, ENABLE);

	/* Enable the CC1 Interrupt Request */
	TIM_ITConfig(TIM5, TIM_IT_CC1 | TIM_IT_CC2, ENABLE);

	// TIM2 Setup

	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);

	TIM_TimeBaseStructure.TIM_Period = 840-1;
	TIM_TimeBaseStructure.TIM_Prescaler = 1-1;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	/* Enable the Interrupt Request */
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	TIM_Cmd(TIM2, ENABLE);
}


void ADC_Configuration(void)
{
	ADC_InitTypeDef ADC_InitStruct;
	ADC_CommonInitTypeDef ADC_CommonInitStruct;
	DMA_InitTypeDef DMA_InitStruct;

	DMA_InitStruct.DMA_BufferSize = 1;
	DMA_InitStruct.DMA_Channel = DMA_Channel_0;
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStruct.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStruct.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
	DMA_InitStruct.DMA_Memory0BaseAddr = (uint32_t)&ADC1_val;
	DMA_InitStruct.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Disable;
	DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t) &ADC1->DR;
	DMA_InitStruct.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;
	DMA_Init(DMA2_Stream4, &DMA_InitStruct);
	DMA_Cmd(DMA2_Stream4, ENABLE);

	ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
	ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
	ADC_CommonInit(&ADC_CommonInitStruct);

	ADC_InitStruct.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConvEdge_None;
	ADC_InitStruct.ADC_NbrOfConversion = 1;
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b;
	ADC_InitStruct.ADC_ScanConvMode = DISABLE;
	ADC_Init(ADC1, &ADC_InitStruct);

	ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_3Cycles);
	ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
	ADC_DMACmd(ADC1, ENABLE);
	ADC_Cmd(ADC1, ENABLE);

	ADC_SoftwareStartConv(ADC1);
}

void RNG_Configuration(void)
{
	RNG_Cmd(ENABLE);
}

/*
 * Callback used by stm32f4_discovery_audio_codec.c.
 * Refer to stm32f4_discovery_audio_codec.h for more info.
 */
void EVAL_AUDIO_TransferComplete_CallBack(uint32_t pBuffer, uint32_t Size){
	/* TODO, implement your code here */
	return;
}

/*
 * Callback used by stm324xg_eval_audio_codec.c.
 * Refer to stm324xg_eval_audio_codec.h for more info.
 */
uint16_t EVAL_AUDIO_GetSampleCallBack(void){
	/* TODO, implement your code here */
	return -1;
}
