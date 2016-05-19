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
#include "misc.h"
#include "delay.h"
#include "codec.h"
#include "List.h"
#include "ff.h"
#include <stdbool.h>

FATFS fatfs;
FIL file;
u16 sample_buffer[512];
volatile u16 result_of_conversion=0;
volatile u8 diode_state=0;
volatile u8 change_song=0;
volatile u8 error_state=0;
void EXTI0_IRQHandler(void)
{
	// drgania stykow
	if(EXTI_GetITStatus(EXTI_Line0) != RESET)
	{
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line0);
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
		// miejsce na kod wywolywany w momencie wystapienia przerwania
		// drgania stykow + zmiana piosenki
		change_song=1;// 1 - wcisnieto przycisk, by zmienic utwor
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
	// TIMER DO ELIMINACJI DRGAN STYKOW
	// TIM5
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	TIM_TimeBaseInitTypeDef TIMER_5;
	/* Time base configuration */
	TIMER_5.TIM_Period = 8400-1;
	TIMER_5.TIM_Prescaler = 2000-1;
	TIMER_5.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMER_5.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM5, &TIMER_5);
	TIM_Cmd(TIM5,DISABLE);

	// KONFIGURACJA PRZERWAN - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure3;
	NVIC_InitStructure3.NVIC_IRQChannel = TIM5_IRQn;// numer przerwania
	NVIC_InitStructure3.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet glowny
	NVIC_InitStructure3.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure3.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kanal
	NVIC_Init(&NVIC_InitStructure3);// zapisz wypelniona strukture do rejestrow
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
	TIMER_3.TIM_Period = 42000-1;// okres zliczania nie przekroczyc 2^16!
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
	GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
	if(diode_state==3)
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_12);
		diode_state=0;
	}
	else if(diode_state==2)
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_13);
		diode_state=3;
	}
	else if(diode_state==1)
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_14);
		diode_state=2;
	}
	else if(diode_state==0)
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_15);
		diode_state=1;
	}
}
void BUTTON_init()
{
	GPIO_InitTypeDef USER_BUTTON;
	USER_BUTTON.GPIO_Pin = GPIO_Pin_0;
	USER_BUTTON.GPIO_Mode = GPIO_Mode_IN;
	USER_BUTTON.GPIO_PuPd = GPIO_PuPd_NOPULL;
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
}
void MY_DMA_initM2P()
{
	DMA_InitTypeDef  DMA_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
	DMA_DeInit(DMA1_Stream5);
	DMA_InitStructure.DMA_Channel = DMA_Channel_0;// wybor kanalu DMA
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;// ustalenie rodzaju transferu (memory2memory / peripheral2memory /memory2peripheral)
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// tryb pracy - pojedynczy transfer badz powtarzany
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;// ustalenie priorytetu danego kanalu DMA
	DMA_InitStructure.DMA_BufferSize = 512;// liczba danych do przeslania
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
	it_status = RESET;
	while(it_status == RESET)
	{
		it_status = DMA_GetFlagStatus(DMA1_Stream5, DMA_FLAG);
	}
	fresult = f_read (&file,&sample_buffer[position],256*2,&read_bytes);
	DMA_ClearFlag(DMA1_Stream5, DMA_FLAG);
	if(fresult != FR_OK)// jesli wyjeto karte w trakcie odtwarzania plikow
	{
		error_state=2;
		return 0;
	}
	if(read_bytes<256*2||change_song==1)
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
		TIM_Cmd(TIM3, ENABLE);
		while(1)
		{
			if (read_and_send(fresult,0, it_status, read_bytes, DMA_FLAG_HTIF5)==0)
			{
				break;
			}
			if (read_and_send(fresult,256, it_status, read_bytes, DMA_FLAG_TCIF5)==0)
			{
				break;
			}
		}
		diode_state=0;
		TIM_Cmd(TIM3, DISABLE);
		fresult = f_close(&file);
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
int main( void )
{
	SystemInit();
	DIODES_init();// inicjalizacja diod
	ADC_init();
	delay_init( 80 );// wyslanie 80 impulsow zegarowych; do inicjalizacji SPI
	SPI_SD_Init();// inicjalizacja SPI pod SD

	// SysTick_CLK... >> taktowany z f = ok. 82MHz/8 = ok. 10MHz
	// Systick_Config >> generuje przerwanie co <10ms
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);// zegar 24-bitowy
	SysTick_Config(90000);

	BUTTON_init();
	JOINT_VIBRATION();
	INTERRUPT_init();
	DIODES_INTERRUPT();
	ERROR_TIM_4();

	// SD CARD
	FRESULT fresult;
	DIR Dir;
	FILINFO fileInfo;

	codec_init();
	codec_ctrl_init();

	I2S_Cmd(CODEC_I2S, ENABLE);// Integrated Interchip Sound to connect digital devices

	struct List *first=0,*last=0,*pointer;
	bool is_the_first_element=0;

	disk_initialize(0);// inicjalizacja karty
	fresult = f_mount( &fatfs, 1,1 );// zarejestrowanie dysku logicznego w systemie
	if(fresult != FR_OK) //jesli wystapil blad tj. wlaczenie STM32 bez karty w module, zle podpiete kable
	{
		error_state=1;
		TIM_Cmd(TIM4, ENABLE);
		for(;;)
		{ }
	}
	fresult = f_opendir(&Dir, "\\");
	if(fresult != FR_OK)
	{
		return(fresult);
	}
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
			if(is_the_first_element==0)
			{
				first=last=add_last(last,fileInfo);
				is_the_first_element++;
			}
			else
			{
				last=add_last(last,fileInfo);
			}
		}
	}
	if (first==0)// jesli na karcie nie ma plikow .wav
	{
		error_state=3;
		TIM_Cmd(TIM4, ENABLE);
		for(;;)
		{ }
	}
	last->next=first;
	first->previous=last;
	pointer=first;
	MY_DMA_initM2P();
	for(;;)
	{
		play_wav(pointer, fresult);
		if(error_state!=0)
		{
			break;
		}
		//pointer=pointer->previous;// do zaktualizowania przy dodatkowych buttonach
		pointer=pointer->next;
	}
	GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15|CODEC_RESET_PIN);
	TIM_Cmd(TIM2,DISABLE);
	TIM_Cmd(TIM4, ENABLE);
	for(;;)
	{ }
	return 0;
}
void SysTick_Handler()
{
	disk_timerproc();
}
