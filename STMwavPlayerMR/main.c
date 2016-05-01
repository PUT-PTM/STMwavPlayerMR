#include "stm32f4xx.h"
#include "system_stm32f4xx.h"
#include "delay.h"
#include "ff.h"
#include "List.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "misc.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_syscfg.h"
#include "codec.h"
#include "stm32f4xx_tim.h"
#include "stm32f4xx_dma.h"
#include <stdbool.h>

FATFS fatfs;
FIL file;
u32 sample_buffer[512];
volatile int diode_state=0;
volatile int change_song=0;

void TIM3_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		spin_diodes();
		// wyzerowanie flagi wyzwolonego przerwania
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	}
}
void EXTI0_IRQHandler(void)
{
	//drgania stykow
	if(EXTI_GetITStatus(EXTI_Line0) != RESET)
	{
		TIM_Cmd(TIM5, ENABLE);
		EXTI_ClearITPendingBit(EXTI_Line0);
	}
}
void TIM5_IRQHandler(void)
{
	if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)
	{
		//miejsce na kod wywolywany w momencie wystapienia przerwania
		//drgania stykow+zmiana piosenki
		change_song=1;//1 - wcisnieto przycisk, by zmienic utwor
		TIM_Cmd(TIM5, DISABLE);
		TIM_SetCounter(TIM5, 0);
		// wyzerowanie flagi wyzwolonego przerwania
		TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
	}
}
void JOINT_VIBRATION()
{
	//TIMER DO ELIMINACJI DRGAÑ STYKÓW
	//TIM5
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	TIM_TimeBaseInitTypeDef TIMER_5;
	/* Time base configuration */
	TIMER_5.TIM_Period = 8400-1;
	TIMER_5.TIM_Prescaler = 2000-1;
	TIMER_5.TIM_ClockDivision = TIM_CKD_DIV1;
	TIMER_5.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM5, &TIMER_5);
	TIM_Cmd(TIM5,DISABLE);

	//KONFIGURACJA PRZERWAÑ - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure3;
	NVIC_InitStructure3.NVIC_IRQChannel = TIM5_IRQn;// numer przerwania
	NVIC_InitStructure3.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet g³ówny
	NVIC_InitStructure3.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure3.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kana³
	NVIC_Init(&NVIC_InitStructure3);// zapisz wype³nion¹ strukturê do rejestrów
	TIM_ClearITPendingBit(TIM5, TIM_IT_Update);// wyczyszczenie przerwania od timera 3 (wyst¹pi³o przy konfiguracji timera)
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepe³nienia dla timera 5
}
void DIODES_INTERRUPT()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
	//KONFIGURACJA PRZERWAÑ - TIMER/COUNTER
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;// numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet g³ówny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kana³
	NVIC_Init(&NVIC_InitStructure);// zapisz wype³nion¹ strukturê do rejestrów

	TIM_TimeBaseInitTypeDef TIMER_3;
	TIMER_3.TIM_Period = 42000-1;//okres zliczania nie przekroczyæ 2^16!
	TIMER_3.TIM_Prescaler = 1000-1;//wartosc preskalera, tutaj bardzo ma³a
	TIMER_3.TIM_ClockDivision = TIM_CKD_DIV1;//dzielnik zegara
	TIMER_3.TIM_CounterMode = TIM_CounterMode_Up;//kierunek zliczania
	TIM_TimeBaseInit(TIM3, &TIMER_3);

	//UWAGA: uruchomienie zegara jest w przerwaniu
	TIM_ClearITPendingBit(TIM3, TIM_IT_Update);// wyczyszczenie przerwania od timera 3 (wyst¹pi³o przy konfiguracji timera)
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);// zezwolenie na przerwania od przepe³nienia dla timera 3
}
void DIODES_init()
{
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	GPIO_InitTypeDef  DIODES;
	/* Configure PD12, PD13, PD14 and PD15 in output pushpull mode */
	DIODES.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
	DIODES.GPIO_Mode = GPIO_Mode_OUT;//tryb wyprowadzenia, wyjcie binarne
	DIODES.GPIO_OType = GPIO_OType_PP;//wyjcie komplementarne
	DIODES.GPIO_Speed = GPIO_Speed_100MHz;//max. V prze³¹czania wyprowadzeñ
	DIODES.GPIO_PuPd = GPIO_PuPd_NOPULL;//brak podci¹gania wyprowadzenia
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
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef USER_BUTTON;
	USER_BUTTON.GPIO_Pin = GPIO_Pin_0;
	USER_BUTTON.GPIO_Mode = GPIO_Mode_IN;
	USER_BUTTON.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOA, &USER_BUTTON);
}
void INTERRUPT_init()
{
	//KONFIGURACJA KONTROLERA PRZERWAÑ
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn; // numer przerwania
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet g³ówny
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;// subpriorytet
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;// uruchom dany kana³
	NVIC_Init(&NVIC_InitStructure);// zapisz wype³nion¹ strukturê do rejestrów

	EXTI_InitTypeDef EXTI_InitStructure;
	EXTI_InitStructure.EXTI_Line = EXTI_Line0;// wybór numeru aktualnie konfigurowanej linii przerwañ
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;// wybór trybu - przerwanie b¹dŸ zdarzenie
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;// wybór zbocza, na które zareaguje przerwanie
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;// uruchom dan¹ liniê przerwañ
	EXTI_Init(&EXTI_InitStructure);// zapisz strukturê konfiguracyjn¹ przerwañ zewnêtrznych do rejestrów

	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
}
void MY_DMA_initM2P()
{
	DMA_InitTypeDef  DMA_InitStructure;
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1, ENABLE);
	DMA_DeInit(DMA1_Stream5);
	DMA_InitStructure.DMA_Channel = DMA_Channel_0; 							// wybór kana³u DMA
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;					// ustalenie rodzaju transferu (memory2memory / peripheral2memory /memory2peripheral)
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;							// tryb pracy - pojedynczy transfer b¹dŸ powtarzany
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;						// ustalenie priorytetu danego kana³u DMA
	DMA_InitStructure.DMA_BufferSize = 512;									// liczba danych do przes³ania
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&sample_buffer;		// adres Ÿród³owy
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(SPI3->DR));		// adres docelowy
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;					// zezwolenie na inkrementacje adresu po ka¿dej przes³anej paczce danych
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Word;			// ustalenie rozmiaru przesy³anych danyc
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;		// ustalenie trybu pracy - jednorazwe przes³anie danych
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;					// wy³¹czenie kolejki FIFO (nie u¿ywana w tym przykadzie
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;

	DMA_Init(DMA1_Stream5, &DMA_InitStructure); // zapisanie wype³nionej struktury do rejestrów wybranego po³¹czenia DMA
	DMA_Cmd(DMA1_Stream5, ENABLE);				// uruchomienie odpowiedniego po³¹czenia DMA

	SPI_I2S_DMACmd(SPI3,SPI_I2S_DMAReq_Tx,ENABLE);
	SPI_Cmd(SPI3,ENABLE);
}
bool read_and_send(int position, volatile ITStatus it_status, UINT read_bytes, uint32_t DMA_FLAG)
{
	it_status = RESET;
	while(it_status == RESET)
	{
		it_status = DMA_GetFlagStatus(DMA1_Stream5, DMA_FLAG);
	}
	f_read (&file,&sample_buffer[position],256*4,&read_bytes);
	DMA_ClearFlag(DMA1_Stream5, DMA_FLAG);
	if(read_bytes<256*4||change_song==1)
	{
		return 0;
	}
	return 1;
}
void play_wav(struct List *song, FRESULT fresult)
{
	struct List *temporary_song=song;//pomocniczo, by nie dzialac na oryginale
	UINT read_bytes;//uzyta w f_read
	fresult = f_open( &file, temporary_song->file.fname, FA_READ );
	if( fresult == FR_OK )
	{
		GPIO_ToggleBits(GPIOD, GPIO_Pin_13);//d.pomaranczowa
		fresult=f_lseek(&file,44);//pominiecie 44 B nag³owka pliku .wav
		volatile ITStatus it_status;//sprawdza flage DMA
		change_song=0;
		TIM_Cmd(TIM3, ENABLE);
		while(1)
		{
			if (read_and_send(0, it_status, read_bytes, DMA_FLAG_HTIF5)==0)
			{
				break;
			}
			if (read_and_send(256, it_status, read_bytes, DMA_FLAG_TCIF5)==0)
			{
				break;
			}
		}
		diode_state=0;
		TIM_Cmd(TIM3, DISABLE);
		fresult = f_close(&file);
	}
}
int main( void )
{
	SystemInit();
	DIODES_init();//inicjacja diod
	delay_init( 80 );//wys³anie 80 impulsow zegarowych; do inicjalizacji SPI
	SPI_SD_Init();//inicjalizacja SPI pod SD

	//*********************************************************************
	//							SysTick
	//SysTick_CLK... >> taktowany z f = 72MHz/8 = 9MHz
	//Systick_Config >> generuje przerwanie co 10ms
	//*********************************************************************
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);	//zegar 24-bitowy
	SysTick_Config(90000);

	BUTTON_init();
	JOINT_VIBRATION();
	INTERRUPT_init();
	DIODES_INTERRUPT();
	//*********************************************************************
	//  						SD CARD
	//*********************************************************************
	FRESULT fresult;
	DIR Dir;
	FILINFO fileInfo;

	codec_init();
	codec_ctrl_init();

	I2S_Cmd(CODEC_I2S, ENABLE);//Integrated Interchip Sound to connect digital devices

	struct List *first=0,*last=0;
	int is_the_first_element=-1;

	disk_initialize(0);//inicjalizacja karty
	fresult = f_mount( &fatfs, 1,1 );//zarejestrowanie dysku logicznego w systemie

	GPIO_SetBits(GPIOD, GPIO_Pin_15);//zapalenie niebieskiej diody

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
		if(is_the_first_element==-1)//pominiecie folderu systemowego znajduj¹cego siê na karcie
		{
			is_the_first_element=0;
		}
		else if(is_the_first_element==0)
		{
			first=last=add_last(last,fileInfo);
			is_the_first_element++;
		}
		else
		{
			last=add_last(last,fileInfo);
		}
	}
	last->next=first;
	GPIO_SetBits(GPIOD, GPIO_Pin_14);
	Codec_VolumeCtrl(138);//ustawienie glosnosci, do ulepszenia, od 0 - 255
	MY_DMA_initM2P();
	for(;;)
	{
		play_wav(first, fresult);
		first=first->next;
	}
	return 0;
}

void SysTick_Handler()
{
	disk_timerproc();
}
