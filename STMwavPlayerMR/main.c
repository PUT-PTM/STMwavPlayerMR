#include "stm32f4xx.h"
#include "system_stm32f4xx.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_dma.h"
#include "stm32f4xx_adc.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_rng.h"
#include "misc.h"
#include "delay.h"
#include "codec.h"
#include "List.h"
#include "ff.h"
#include <stdbool.h>
#include "tm_stm32f4_pcd8544.h"

FATFS fatfs;
FIL file;
u16 sample_buffer[2048];
volatile s8 num_of_switch=-1;
volatile u16 result_of_conversion=0;
volatile u8 diode_state=0;
volatile s8 change_song=0;
volatile u8 error_state=0;
volatile bool random_mode=0;
bool pause=0;
char song_time[5]={'0', '0', ':', '0', '0'};
bool half_second=0;

void EXTI0_IRQHandler(void)
{
	// drgania stykow
	if(EXTI_GetITStatus(EXTI_Line0) != RESET)
	{
		num_of_switch=0;
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line0);
	}
}
void EXTI9_5_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line5) != RESET)
	{
		num_of_switch=5;
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line5);
	}
	else if(EXTI_GetITStatus(EXTI_Line7) != RESET)
	{
		num_of_switch=7;
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line7);
	}
	else if(EXTI_GetITStatus(EXTI_Line8) != RESET)
	{
		num_of_switch=8;
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line8);
	}
}
void TIM2_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
	{
		ADC_conversion();
		Codec_VolumeCtrl(result_of_conversion);
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		if(half_second==false)
		{
			half_second=true;
		}
		else
		{
			half_second=false;
		}
		if(half_second==0)
		{
			song_time[4]++;
			if(song_time[4]==':')
			{
				song_time[3]++;
				song_time[4]='0';
			}
			if(song_time[3]=='6')
			{
				song_time[1]++;
				song_time[3]='0';
			}
			if(song_time[1]==':')
			{
				song_time[0]++;
				song_time[1]='0';
			}
			if(song_time[0]==':')
			{
				song_time[0]='0';
			}
		}
		spin_diodes();
		// wyzerowanie flagi wyzwolonego przerwania
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}
void TIM4_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
	{
		if (error_state==1)// jesli uruchomiono program bez karty SD w module, zle podpieto kable
		{
			GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
		}
		if (error_state==2)// jesli wyjeto karte SD w trakcie odtwarzania plikow
		{
			GPIO_ToggleBits(GPIOD, GPIO_Pin_13);
		}
		if (error_state==3)// jesli na karcie SD nie ma plikow .wav
		{
			GPIO_ToggleBits(GPIOD, GPIO_Pin_14);
		}
		if (error_state==4)// niezagospodarowane na obecna chwile
		{
			GPIO_ToggleBits(GPIOD, GPIO_Pin_15);
		}
		// wyzerowanie flagi wyzwolonego przerwania
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
void TIM5_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)
	{
		// miejsce na kod wywolywany w momencie wystapienia przerwania, drgania stykow
		if (num_of_switch==0)// wcisnieto user button 0 - losowe odtwarzanie
		{
			random_mode = (random_mode + 1) % 2;
		}
		else if (num_of_switch==5)// wcisnieto switch 5 - przewijanie do przodu
		{
			change_song=1;
		}
		else if (num_of_switch==7)// wcisnieto switch 7 - pauzuj/wznow
		{
			if(pause==0)
			{
				pause=1;
				TIM_Cmd(TIM3,DISABLE);
				Codec_PauseResume(0);
				NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE);
			}
			else
			{
				pause=0;
				TIM_Cmd(TIM3,ENABLE);
				Codec_PauseResume(1);
				NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, DISABLE);
			}
		}
		else if (num_of_switch==8)// wcisnieto switch 8 - przewijanie wstecz
		{
			change_song=-1;
		}
		num_of_switch=-1;
		TIM_Cmd(TIM5, DISABLE);
		TIM_SetCounter(TIM5, 0);
		// wyzerowanie flagi wyzwolonego przerwania
		TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
	}
}
void ERROR_TIM_4()
{
	// TIMER DO OBSLUGI DIOD W PRZYPADKU BLEDU
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	TIM_TimeBaseInitTypeDef TIMER_4;
	/* Time base configuration */
	TIMER_4.TIM_Period = 24000-1;
	TIMER_4.TIM_Prescaler = 1000-1;
	TIMER_4.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMER_4.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIMER_4);
	TIM_Cmd(TIM4,DISABLE);

	// KONFIGURACJA PRZERWAN - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;// numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure);// zapisz wypelniona strukture do rejestrow
	TIM_ClearITPendingBit(TIM4, TIM_IT_Update);// wyczyszczenie przerwania od timera 4 (wystapilo przy konfiguracji timera)
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepelnienia dla timera 4
}
void JOINT_VIBRATION()
{
	// TIMER DO ELIMINACJI DRGAN STYKOW, TIM5
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);

	TIM_TimeBaseInitTypeDef TIMER;
	/* Time base configuration */
	TIMER.TIM_Period = 8400-1;
	TIMER.TIM_Prescaler = 3000-1;
	TIMER.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMER.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM5, &TIMER);
	TIM_Cmd(TIM5,DISABLE);

	// KONFIGURACJA PRZERWAN - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;// numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure);// zapisz wypelniona strukture do rejestrow
	TIM_ClearITPendingBit(TIM5, TIM_IT_Update);// wyczyszczenie przerwania od timera 5 (wystapilo przy konfiguracji timera)
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepelnienia dla timera 5
}
void DIODES_INTERRUPT()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	// KONFIGURACJA PRZERWAN - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;// numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure);// zapisz wypelniona strukture do rejestrow

	TIM_TimeBaseInitTypeDef TIMER_3;
	TIMER_3.TIM_Period = 48000-1;// okres zliczania nie przekroczyc 2^16!
	TIMER_3.TIM_Prescaler = 1000-1;// wartosc preskalera, tutaj bardzo mala
	TIMER_3.TIM_ClockDivision = TIM_CKD_DIV1;// dzielnik zegara
	TIMER_3.TIM_CounterMode = TIM_CounterMode_Up;// kierunek zliczania
	TIM_TimeBaseInit(TIM3, &TIMER_3);

	// UWAGA: uruchomienie zegara jest w przerwaniu
	TIM_ClearITPendingBit(TIM3, TIM_IT_Update);// wyczyszczenie przerwania od timera 3 (wystapilo przy konfiguracji timera)
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepelnienia dla timera 3
}
void DIODES_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	GPIO_InitTypeDef  DIODES;
	/* Configure PD12, PD13, PD14 and PD15 in output pushpull mode */
	DIODES.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
	DIODES.GPIO_Mode = GPIO_Mode_OUT;// tryb wyprowadzenia, wyjcie binarne
	DIODES.GPIO_OType = GPIO_OType_PP;// wyjcie komplementarne
	DIODES.GPIO_Speed = GPIO_Speed_100MHz;// max. V przelaczania wyprowadzen
	DIODES.GPIO_PuPd = GPIO_PuPd_NOPULL;// brak podciagania wyprowadzenia
	GPIO_Init(GPIOD, &DIODES);
}
void spin_diodes()
{
	if(random_mode==0)
	{
		GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
	}
	else
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
	}

	if(diode_state==3)
	{
		if(random_mode==0)
		{
			GPIO_SetBits(GPIOD, GPIO_Pin_12);
		}
		else
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12);
		}
		diode_state=0;
	}
	else if(diode_state==2)
	{
		if(random_mode==0)
		{
			GPIO_SetBits(GPIOD, GPIO_Pin_13);
		}
		else
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_13);
		}
		diode_state=3;
	}
	else if(diode_state==1)
	{
		if(random_mode==0)
		{
			GPIO_SetBits(GPIOD, GPIO_Pin_14);
		}
		else
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_14);
		}
		diode_state=2;
	}
	else if(diode_state==0)
	{
		if(random_mode==0)
		{
			GPIO_SetBits(GPIOD, GPIO_Pin_15);
		}
		else
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_15);
		}
		diode_state=1;
	}
}
void BUTTON_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA , ENABLE);
	/*0 - tryb losowy
	  5 - przewijanie wstecz
	  6 - start/stop
	  7 - przewijanie do przodu*/
	GPIO_InitTypeDef USER_BUTTON;
	USER_BUTTON.GPIO_Pin = GPIO_Pin_0;
	USER_BUTTON.GPIO_Mode = GPIO_Mode_IN;
	USER_BUTTON.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &USER_BUTTON);

	USER_BUTTON.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7 | GPIO_Pin_8;
	USER_BUTTON.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_Init(GPIOA, &USER_BUTTON);
}
void INTERRUPT_init()
{
	// KONFIGURACJA KONTROLERA PRZERWAN
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn; // numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure);// zapisz wypelniona strukture do rejestrow

	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line = EXTI_Line0;// wybor numeru aktualnie konfigurowanej linii przerwan
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;// wybor trybu - przerwanie badz zdarzenie
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;// wybor zbocza, na ktore zareaguje przerwanie
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;// uruchom dana linie przerwan
	EXTI_Init(&EXTI_InitStructure);// zapisz strukture konfiguracyjna przerwan zewnetrznych do rejestrow

	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

	// KONFIGURACJA KONTROLERA PRZERWAN DLA SWITCH Pin_5, Pin_6, Pin_7
	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	EXTI_InitStructure.EXTI_Line = EXTI_Line5 | EXTI_Line7 | EXTI_Line8;
	NVIC_Init(&NVIC_InitStructure);
	EXTI_Init(&EXTI_InitStructure);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource5);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource7);
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource8);
}
void MY_DMA_initM2P()
{
	DMA_InitTypeDef  DMA_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
	DMA_DeInit(DMA1_Stream5);
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;// wybor kanalu DMA
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;// ustalenie rodzaju transferu (memory2memory / peripheral2memory /memory2peripheral)
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// tryb pracy - pojedynczy transfer badz powtarzany
	DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;// ustalenie priorytetu danego kanalu DMA
	DMA_InitStructure.DMA_BufferSize = 512*4;// liczba danych do przeslania
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&sample_buffer;// adres zrodlowy
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI3->DR));// adres docelowy
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;// zezwolenie na inkrementacje adresu po kazdej przeslanej paczce danych
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;// ustalenie rozmiaru przesylanych danych
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;// ustalenie trybu pracy - jednorazwe przeslanie danych
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;// wylaczenie kolejki FIFO (nie uzywana w tym przykadzie
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;

	DMA_Init(DMA1_Stream5, &DMA_InitStructure);// zapisanie wypelnionej struktury do rejestrow wybranego polaczenia DMA
	DMA_Cmd(DMA1_Stream5, ENABLE);// uruchomienie odpowiedniego polaczenia DMA

	SPI_I2S_DMACmd(SPI3,SPI_I2S_DMAReq_Tx,ENABLE);
	SPI_Cmd(SPI3,ENABLE);
}
void ADC_conversion()
{
	// Odczyt wartosci przez odpytnie flagi zakonczenia konwersji
	// Wielorazowe sprawdzenie wartoci wyniku konwersji
	ADC_SoftwareStartConv(ADC1);
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);
	result_of_conversion = ((ADC_GetConversionValue(ADC1))/16);
}
void TIM2_ADC_init()
{
	// Wejscie do przerwania od TIM2 co <0.05 s
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	// 2. UTWORZENIE STRUKTURY KONFIGURACYJNEJ
	TIM_TimeBaseInitTypeDef TIMER_2;
	TIMER_2.TIM_Period = 2100-1;// okres zliczania nie przekroczyc 2^16!
	TIMER_2.TIM_Prescaler = 2000-1;// wartosc preskalera, tutaj bardzo mala
	TIMER_2.TIM_ClockDivision = TIM_CKD_DIV1;// dzielnik zegara
	TIMER_2.TIM_CounterMode = TIM_CounterMode_Up;// kierunek zliczania
	TIM_TimeBaseInit(TIM2, &TIMER_2);
	TIM_Cmd(TIM2, ENABLE);// Uruchomienie Timera

	// KONFIGURACJA PRZERWAN - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;// numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure);// zapisz wypelniona strukture do rejestrow
	TIM_ClearITPendingBit(TIM2, TIM_IT_Update);// wyczyszczenie przerwania od timera 2 (wystapilo przy konfiguracji timera)
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepelnienia dla timera 2
}
void ADC_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA , ENABLE);// zegar dla portu GPIO z ktorego wykorzystany zostanie pin
	// jako wejscie ADC (PA1)
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);// zegar dla modulu ADC1

	// inicjalizacja wejscia ADC
	GPIO_InitTypeDef  GPIO_InitStructureADC;
	GPIO_InitStructureADC.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructureADC.GPIO_Mode = GPIO_Mode_AN;
	GPIO_InitStructureADC.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &GPIO_InitStructureADC);

	ADC_CommonInitTypeDef ADC_CommonInitStructure;// Konfiguracja dla wszystkich ukladow ADC
	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;// niezalezny tryb pracy przetwornikow
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div2;// zegar glowny podzielony przez 2
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;// opcja istotna tylko dla tryby multi ADC
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;// czas przerwy pomiedzy kolejnymi konwersjami
	ADC_CommonInit(&ADC_CommonInitStructure);

	ADC_InitTypeDef ADC_InitStructure;// Konfiguracja danego przetwornika
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;// ustawienie rozdzielczosci przetwornika na maksymalna (12 bitow)
	// wylaczenie trybu skanowania (odczytywac bedziemy jedno wejscie ADC
	// w trybie skanowania automatycznie wykonywana jest konwersja na wielu
	// wejsciach/kanalach)
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;// wlaczenie ciaglego trybu pracy wylaczenie zewnetrznego wyzwalania
	// konwersja moze byc wyzwalana timerem, stanem wejscia itd. (szczegoly w dokumentacji)
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	// wartosc binarna wyniku bedzie podawana z wyrownaniem do prawej
	// funkcja do odczytu stanu przetwornika ADC zwraca wartosc 16-bitowa
	// dla przykladu, wartosc 0xFF wyrownana w prawo to 0x00FF, w lewo 0x0FF0
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfConversion = 1;// liczba konwersji rowna 1, bo 1 kanal
	ADC_Init(ADC1, &ADC_InitStructure);// zapisz wypelniona strukture do rejestrow przetwornika numer 1

	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_84Cycles);// Konfiguracja kanalu pierwszego ADC
	ADC_Cmd(ADC1, ENABLE);// Uruchomienie przetwornika ADC

	TIM2_ADC_init();
}
bool read_and_send(FRESULT fresult, int position, volatile ITStatus it_status, UINT read_bytes, uint32_t DMA_FLAG)
{
	PCD8544_Clear_time();
	PCD8544_GotoXY(29, 27);
	PCD8544_Puts(song_time, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_Refresh();

	it_status = RESET;
	while(it_status == RESET)
	{
		it_status = DMA_GetFlagStatus(DMA1_Stream5, DMA_FLAG);
	}
	fresult = f_read (&file,&sample_buffer[position],1024*2,&read_bytes);
	DMA_ClearFlag(DMA1_Stream5, DMA_FLAG);

	if(fresult != FR_OK)// jesli wyjeto karte w trakcie odtwarzania plikow
	{
		error_state=2;
		return 0;
	}
	if(read_bytes<1024*2||change_song!=0)
	{
		return 0;
	}
	return 1;
}
void play_wav(struct List *song, FRESULT fresult)
{
	struct List *temporary_song=song;// pomocniczo, by nie dzialac na oryginale
	UINT read_bytes;// uzyta w f_read
	fresult = f_open( &file, temporary_song->file.fname, FA_READ );
	if( fresult == FR_OK )
	{
		fresult=f_lseek(&file,44);// pominiecie 44 B naglowka pliku .wav
		volatile ITStatus it_status;// sprawdza flage DMA
		change_song=0;
		song_time[0]=song_time[1]=song_time[3]=song_time[4]='0';
		song_time[2]=':';
		half_second=0;
		display_const();
		PCD8544_GotoXY(8, 15);
		PCD8544_Puts(temporary_song->file.fname, PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		TIM_Cmd(TIM3, ENABLE);
		while(1)
		{
			if (read_and_send(fresult,0, it_status, read_bytes, DMA_FLAG_HTIF5)==0)
			{
				break;
			}
			if (read_and_send(fresult, 1024, it_status, read_bytes, DMA_FLAG_TCIF5)==0)
			{
				break;
			}
		}
		diode_state=0;
		TIM_Cmd(TIM3, DISABLE);
		fresult = f_close(&file);
		PCD8544_Clear();
	}
}
bool isWAV(FILINFO fileInfo)
{
	int i=0;
	for (i=0;i<10;i++)
	{
		if(fileInfo.fname[i]=='.')
		{
			if(fileInfo.fname[i+1]=='W' && fileInfo.fname[i+2]=='A' && fileInfo.fname[i+3]=='V')
			{
				return 1;
			}
		}
	}
	return 0;
}
void display_const()
{
	//Go to x=1, y=2 position
	PCD8544_GotoXY(1, 2);
	//Print data with Pixel Set mode and Fontsize of 5x7px
	PCD8544_Puts("STMwavPlayerMR", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_GotoXY(31, 40);
	PCD8544_Puts("2016", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_Refresh();
}
int main( void )
{
	SystemInit();
	DIODES_init();// inicjalizacja diod
	ERROR_TIM_4();
	delay_init( 80 );// wyslanie 80 impulsow zegarowych; do inicjalizacji SPI
	SPI_SD_Init();// inicjalizacja SPI pod SD

	// SysTick_CLK... >> taktowany z f = ok. 82MHz/8 = ok. 10MHz
	// Systick_Config >> generuje przerwanie co <10ms
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);// zegar 24-bitowy
	SysTick_Config(90000);

	//Initialize LCD with 0x38 software contrast
	PCD8544_Init(0x38);
	display_const();

	// SD CARD
	FRESULT fresult;
	DIR Dir;
	FILINFO fileInfo;

	struct List *first=0,*last=0,*pointer;

	disk_initialize(0);// inicjalizacja karty
	fresult = f_mount( &fatfs, 1,1 );// zarejestrowanie dysku logicznego w systemie
	if(fresult != FR_OK) //jesli wystapil blad tj. wlaczenie STM32 bez karty w module, zle podpiete kable
	{
		error_state=1;
		PCD8544_GotoXY(21, 13);
		PCD8544_Puts("Problem", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_GotoXY(17, 21);
		PCD8544_Puts("z kablami", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_GotoXY(17, 29);
		PCD8544_Puts("lub karta", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_Refresh();
		TIM_Cmd(TIM4, ENABLE);
		for(;;)
		{ }
	}
	fresult = f_opendir(&Dir, "\\");
	if(fresult != FR_OK)
	{
		return(fresult);
	}
	u32 number_of_songs=0;
	for(;;)
	{
		fresult = f_readdir(&Dir, &fileInfo);
		if(fresult != FR_OK)
		{
			return(fresult);
		}
		if(!fileInfo.fname[0])
		{
			break;
		}
		if(isWAV(fileInfo)==1)// sprawdzenie, czy plik na karcie ma rozszerzenie .wav
		{
			if(number_of_songs==0)
			{
				first=last=add_last(last,fileInfo);
			}
			else
			{
				last=add_last(last,fileInfo);
			}
			number_of_songs++;
		}
	}
	if (first==0)// jesli na karcie nie ma plikow .wav
	{
		error_state=3;
		PCD8544_GotoXY(12, 17);
		PCD8544_Puts("Brak plikow", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_GotoXY(3, 25);
		PCD8544_Puts(".wav na karcie", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
		PCD8544_Refresh();
		TIM_Cmd(TIM4, ENABLE);
		for(;;)
		{ }
	}
	last->next=first;
	first->previous=last;
	pointer=first;

	codec_init();
	codec_ctrl_init();
	I2S_Cmd(CODEC_I2S, ENABLE);// Integrated Interchip Sound to connect digital devices
	MY_DMA_initM2P();
	BUTTON_init();
	ADC_init();
	JOINT_VIBRATION();
	INTERRUPT_init();
	DIODES_INTERRUPT();
	RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
	RNG_Cmd(ENABLE);
	u32 rand_number=0;
	u32 i_loop=0;

	for(;;)
	{
		play_wav(pointer, fresult);
		if(error_state!=0)
		{
			break;
		}
		if(change_song>=0)// wcisnieto switch 5 albo skonczylo sie odtwarzanie utworu
		{
			if(random_mode==1 && number_of_songs>1)
			{
				rand_number=RNG_GetRandomNumber()%(number_of_songs-1)+1;
				for(i_loop=1;i_loop<=rand_number;i_loop++)
				{
					pointer=pointer->next;
				}
			}
			else
			{
				pointer=pointer->next;
			}
		}
		else if (change_song==-1)
		{
			if(random_mode==1 && number_of_songs>1)
			{
				rand_number=RNG_GetRandomNumber()%(number_of_songs-1)+1;
				for(i_loop=1;i_loop<=rand_number;i_loop++)
				{
					pointer=pointer->previous;
				}
			}
			else
			{
				pointer=pointer->previous;
			}
		}
	}
	GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15|CODEC_RESET_PIN);
	display_const();
	PCD8544_GotoXY(8, 13);
	PCD8544_Puts("Wyjeto karte", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_GotoXY(34, 21);
	PCD8544_Puts("lub", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_GotoXY(5, 29);
	PCD8544_Puts("wypiety kabel", PCD8544_Pixel_Set, PCD8544_FontSize_5x7);
	PCD8544_Refresh();
	TIM_Cmd(TIM2, DISABLE);
	TIM_Cmd(TIM4, ENABLE);
	for(;;)
	{ }
	return 0;
}
void SysTick_Handler()
{
	disk_timerproc();
}
