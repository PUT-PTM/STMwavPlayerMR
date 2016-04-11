#include "stm32f4xx.h"
#include "system_stm32f4xx.h"
#include "delay.h"
#include "fpu.h"
#include "spi_sd.h"
#include "ff.h"
#include "Lista.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "misc.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_syscfg.h"
FATFS fatfs;

void EXTI1_IRQHandler(void)
{
	if(EXTI_GetITStatus(EXTI_Line1) != RESET)
	{
	// miejsce na kod wywo³ywany w momencie wyst¹pienia przerwania
	// wyzerowanie flagi wyzwolonego przerwania
	EXTI_ClearITPendingBit(EXTI_Line1);
	}
}
FIL plik;
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

    fpu_enable(); 		//jednostka zmiennoprzecinkowa, Floating-Point_Unit
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
