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

volatile uint8_t outBuffer[OUTBUFFERSIZE];

volatile uint8_t electrify = 0;		// electric mode
volatile uint32_t duration = 44100;	// controls duration of note
volatile float amplitude = 1.0;
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

/* IRQ Handlers */
void TIM4_IRQHandler(void)
{
   // Clear TIM4 Capture compare interrupt pending bit
   TIM_ClearITPendingBit(TIM4, TIM_IT_CC1);

   // Get the Input Capture value
   IC1Value = TIM_GetCapture1(TIM4);
   amplitude = (float)(volume)*(1-(float)IC1Value/0xFFFF);

   // Let us know that that the string was plucked
   string_plucked = 1;
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
			n = 0;

			// output sound
			while(1)
			{
				// cycle through strings if the string is plucked while note is being played
				// (this is only to check the different strings using only 1 laser; will be removed later)
				if(string_plucked == 1)
				{
					stringNo--;
					if(stringNo == 0)
						stringNo = 6;

					if(stringNo == 6)
					{
						noteFreq = 20.6;
						octave = 2;
					}
					else if(stringNo == 5)
					{
						noteFreq = 27.5;
						octave = 2;
					}
					else if(stringNo == 4)
					{
						noteFreq = 18.35;
						octave = 3;
					}
					else if(stringNo == 3)
					{
						noteFreq = 24.5;
						octave = 3;
					}
					else if(stringNo == 2)
					{
						noteFreq = 30.87;
						octave = 3;
					}
					else if(stringNo == 1)
					{
						noteFreq = 20.6;
						octave = 4;
					}

					// update note
					DACBufferSize = (uint16_t)(((float)44100/(noteFreq*pow(2, octave))));
					if(DACBufferSize & 0x00000001)
						DACBufferSize +=1;
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
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	// enable clock for Random Number Generator
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);

	// enable clock for timer 4
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
}

void GPIO_Configuration(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* TIM4 channel2 configuration : PB.07 */
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOB, &GPIO_InitStructure);

	/* Connect TIM pin to AF2 */
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_TIM4);
}

void NVIC_Configuration(void)
{
	NVIC_InitTypeDef NVIC_InitStructure;

	/* Enable the TIM4 global Interrupt */
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;

	NVIC_Init(&NVIC_InitStructure);
}

void Timer_Configuration(void)
{
	TIM_ICInitTypeDef  TIM_ICInitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

	TIM_TimeBaseStructure.TIM_Period = 65535;
	TIM_TimeBaseStructure.TIM_Prescaler = 2000;
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

	TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
	TIM_ICInitStructure.TIM_ICFilter = 0x0;


	TIM_PWMIConfig(TIM4, &TIM_ICInitStructure);

	/* Select the TIM4 Input Trigger: TI2FP2 */
	TIM_SelectInputTrigger(TIM4, TIM_TS_TI2FP2);

	/* Select the slave Mode: Reset Mode */
	TIM_SelectSlaveMode(TIM4, TIM_SlaveMode_Reset);
	TIM_SelectMasterSlaveMode(TIM4,TIM_MasterSlaveMode_Enable);
	//TIM_SelectOnePulseMode(TIM4, TIM_OPMode_Single);
	/* TIM enable counter */
	TIM_Cmd(TIM4, ENABLE);

	/* Enable the CC1 Interrupt Request */
	TIM_ITConfig(TIM4, TIM_IT_CC1, ENABLE);
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
