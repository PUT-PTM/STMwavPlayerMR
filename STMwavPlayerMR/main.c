#include "stm32f4xx.h"
#include "system_stm32f4xx.h"
#include "delay.h"
#include "spi_sd.h"
#include "ff.h"
#include "Lista.h"
#include "stm32f4xx_conf.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "misc.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_syscfg.h"
#include "codec.h"
#include "stm32f4xx_tim.h"

FATFS fatfs;
FIL plik;
u32 probka_buffer[512];
volatile int stan=0;
volatile int change_song=0;

void TIM3_IRQHandler(void)
{
     if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
     {
    	 po_kolei();
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

	// miejsce na kod wywo³ywany w momencie wyst¹pienia przerwania

	EXTI_ClearITPendingBit(EXTI_Line0);
	}
}
void TIM5_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM5, TIM_IT_Update) != RESET)
    {
    //miejsce na kod wywo³ywany w momencie wyst¹pienia przerwania
    //drgania stykow+zmiana piosenki
    change_song=1;//1 - wcisnieto przycisk, by zmienic utwor
    TIM_Cmd(TIM5, DISABLE);
    TIM_SetCounter(TIM5, 0);
    // wyzerowanie flagi wyzwolonego przerwania
    TIM_ClearITPendingBit(TIM5, TIM_IT_Update);
    }
}
void JOINT_VIBRATION(){
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
	NVIC_InitStructure3.NVIC_IRQChannel = TIM5_IRQn; // numer przerwania
	NVIC_InitStructure3.NVIC_IRQChannelPreemptionPriority = 0x00; // priorytet g³ówny
	NVIC_InitStructure3.NVIC_IRQChannelSubPriority = 0x00; // subpriorytet
	NVIC_InitStructure3.NVIC_IRQChannelCmd = ENABLE; // uruchom dany kana³
	NVIC_Init(&NVIC_InitStructure3); // zapisz wype³nion¹ strukturê do rejestrów
	TIM_ClearITPendingBit(TIM5, TIM_IT_Update); // wyczyszczenie przerwania od timera 3 (wyst¹pi³o przy konfiguracji timera)
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE); // zezwolenie na przerwania od przepe³nienia dla timera 5
}
void DIODES_INTERRUPT(){

		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
		//KONFIGURACJA PRZERWAÑ - TIMER/COUNTER
		NVIC_InitTypeDef NVIC_InitStructure;
		NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn; 			// numer przerwania
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet g³ówny
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00; 		// subpriorytet
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 			// uruchom dany kana³
		NVIC_Init(&NVIC_InitStructure); 							// zapisz wype³nion¹ strukturê do rejestrów

		TIM_TimeBaseInitTypeDef TIMER_3;

		TIMER_3.TIM_Period = 42000-1; 					//okres zliczania nie przekroczyæ 2^16!
		TIMER_3.TIM_Prescaler = 1000-1;  				//wartosc preskalera, tutaj bardzo ma³a
		TIMER_3.TIM_ClockDivision = TIM_CKD_DIV1; 		//dzielnik zegara
		TIMER_3.TIM_CounterMode = TIM_CounterMode_Up; 	//kierunek zliczania
		TIM_TimeBaseInit(TIM3, &TIMER_3);

		//UWAGA: uruchomienie zegara jest w przerwaniu
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update); // wyczyszczenie przerwania od timera 3 (wyst¹pi³o przy konfiguracji timera)
		TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE); // zezwolenie na przerwania od przepe³nienia dla timera 3
}
void DIODES_init(){
	 RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
	 GPIO_InitTypeDef  DIODES;
	 /* Configure PD12, PD13, PD14 and PD15 in output pushpull mode */
	 DIODES.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
	 DIODES.GPIO_Mode = GPIO_Mode_OUT; 		//tryb wyprowadzenia, wyjcie binarne
	 DIODES.GPIO_OType = GPIO_OType_PP; 	//wyjcie komplementarne
	 DIODES.GPIO_Speed = GPIO_Speed_100MHz;	//max. V prze³¹czania wyprowadzeñ
	 DIODES.GPIO_PuPd = GPIO_PuPd_NOPULL;	//brak podci¹gania wyprowadzenia
	 GPIO_Init(GPIOD, &DIODES);
}
void po_kolei()
{
	if(stan==3)
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
			GPIO_SetBits(GPIOD, GPIO_Pin_12);
			stan=0;
		}
	else if(stan==2)
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
			GPIO_SetBits(GPIOD, GPIO_Pin_13);
			stan=3;
			}
	else if(stan==1)
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
			GPIO_SetBits(GPIOD, GPIO_Pin_14);
			stan=2;
		}
	else if(stan==0)
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12|GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);
			GPIO_SetBits(GPIOD, GPIO_Pin_15);
			stan=1;
		}
}
void zalacz_wylacz(){
	if(stan==0 && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)==1)
	{
		GPIO_SetBits(GPIOD, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
		stan=1;
	}
	else if(stan==1 && GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0)==1)
		{
			GPIO_ResetBits(GPIOD, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);
			stan=0;
		}
}
void BUTTON_init(){
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
		GPIO_InitTypeDef  USER_BUTTON;
	    USER_BUTTON.GPIO_Pin = GPIO_Pin_0;
	    USER_BUTTON.GPIO_Mode = GPIO_Mode_IN;
	    USER_BUTTON.GPIO_PuPd = GPIO_PuPd_NOPULL;
	    GPIO_Init(GPIOA, &USER_BUTTON);
}
void INTERRUPT_init(){
	//KONFIGURACJA KONTROLERA PRZERWAÑ
	    NVIC_InitTypeDef NVIC_InitStructure;
	    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn; 			// numer przerwania
	    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;// priorytet g³ówny
	    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00; 		// subpriorytet
	    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; 			// uruchom dany kana³
	    NVIC_Init(&NVIC_InitStructure); 							// zapisz wype³nion¹ strukturê do rejestrów

	    EXTI_InitTypeDef EXTI_InitStructure;
	    EXTI_InitStructure.EXTI_Line = EXTI_Line0; 				// wybór numeru aktualnie konfigurowanej linii przerwañ
	    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt; 	// wybór trybu - przerwanie b¹dŸ zdarzenie
	    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; 	// wybór zbocza, na które zareaguje przerwanie
	    EXTI_InitStructure.EXTI_LineCmd = ENABLE; 				// uruchom dan¹ liniê przerwañ
	    EXTI_Init(&EXTI_InitStructure); 						// zapisz strukturê konfiguracyjn¹ przerwañ zewnêtrznych do rejestrów

	    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
}

play_wav(struct Lista *utwor, FRESULT fresult)
{
	struct Lista *utwor_tymczasowy=utwor;
	WORD bajty_wczytane=0;
	u8 bufor_na_offset[44];		//pomijane dane z pliku .Wav
	fresult = f_open( &plik, utwor_tymczasowy->plik.fname, FA_READ );
	if( fresult == FR_OK )
	{
		GPIO_ToggleBits(GPIOD, GPIO_Pin_13);
		//opuszczamy nag³ówek pliku wav - 44 bajty
		fresult = f_read (&plik, &bufor_na_offset[0], 44, &bajty_wczytane);
	    fresult = f_close( &plik );
	    }

}

int main( void )
{
    SystemInit();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    //*********************************************************************
    //							DIODES
    //*********************************************************************
    GPIO_InitTypeDef  DIODES;
    /* Configure PD12, PD13, PD14 and PD15 in output pushpull mode */
    DIODES.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13| GPIO_Pin_14| GPIO_Pin_15;
    DIODES.GPIO_Mode = GPIO_Mode_OUT; 		//tryb wyprowadzenia, wyjcie binarne
    DIODES.GPIO_OType = GPIO_OType_PP; 		//wyjcie komplementarne
    DIODES.GPIO_Speed = GPIO_Speed_100MHz;	//max. V prze³¹czania wyprowadzeñ
    DIODES.GPIO_PuPd = GPIO_PuPd_NOPULL;	//brak podci¹gania wyprowadzenia
    GPIO_Init(GPIOD, &DIODES);

    delay_init( 80 );	//wys³anie 80 impulsow zegarowych; do inicjalizacji SPI
    SPI_SD_Init();		//inicjalizacja SPI pod SD

    //*********************************************************************
    //							SysTick
    //SysTick_CLK... >> taktowany z f = 72MHz/8 = 9MHz
    //Systick_Config >> generuje przerwanie co 10ms
    //*********************************************************************
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);	//zegar 24-bitowy
    SysTick_Config(90000);

    //*** BUTTON ***
    GPIO_InitTypeDef  USER_BUTTON;
    USER_BUTTON.GPIO_Pin = GPIO_Pin_0;
    USER_BUTTON.GPIO_Mode = GPIO_Mode_IN;
    USER_BUTTON.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &USER_BUTTON);

    //KONFIGURACJA KONTROLERA PRZERWAÑ
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn; // numer przerwania
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00; // priorytet g³ówny
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00; // subpriorytet
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE; // uruchom dany kana³
    NVIC_Init(&NVIC_InitStructure); // zapisz wype³nion¹ strukturê do rejestrów

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line1; // wybór numeru aktualnie konfigurowanej linii przerwañ
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt; // wybór trybu - przerwanie b¹dŸ zdarzenie
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; // wybór zbocza, na które zareaguje przerwanie
    EXTI_InitStructure.EXTI_LineCmd = ENABLE; // uruchom dan¹ liniê przerwañ
    EXTI_Init(&EXTI_InitStructure); // zapisz strukturê konfiguracyjn¹ przerwañ zewnêtrznych do rejestrów

    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);

    //*********************************************************************
    //  						SD CARD
    //*********************************************************************
    FRESULT fresult;

    DIR Dir;
    FILINFO plikInfo;

    struct Lista *first=0,*last=0;
    int czy_pierwszy_elem=-1;

    disk_initialize(0);				 //inicjalizacja karty
    fresult = f_mount( 0, &fatfs );	 //zarejestrowanie dysku logicznego w systemie

    GPIO_SetBits(GPIOD, GPIO_Pin_15);//zapalenie niebieskiej diody

        fresult = f_opendir(&Dir, "\\");

        if(fresult != FR_OK)
          return(fresult);

        for(;;)
        {
          fresult = f_readdir(&Dir, &plikInfo);

          if(fresult != FR_OK)
            return(fresult);
          if(!plikInfo.fname[0])
            break;
          if(czy_pierwszy_elem==-1) //pomijam folder systemowy znajduj¹cy siê na karcie
          {
        	  czy_pierwszy_elem=0;
          }
          else if(czy_pierwszy_elem==0)
          {
        	  first=last=add_last(last,plikInfo);
        	  czy_pierwszy_elem++;
          }
          else
        	  last=add_last(last,plikInfo);

        }
        last->next=first;
        GPIO_SetBits(GPIOD, GPIO_Pin_14);

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
