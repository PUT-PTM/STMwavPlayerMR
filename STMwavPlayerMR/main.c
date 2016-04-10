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

FATFS fatfs;

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

    //*********************************************************************
    //  						SD CARD
    //*********************************************************************
    FRESULT fresult;
    DIR Dir;
    FILINFO plikInfo;
    struct Lista *first=0,*last=0;
    int list_pom=0;

    disk_initialize(0);				 //inicjalizacja karty
    fresult = f_mount( 0, &fatfs );	 //zarejestrowanie dysku logicznego w systemie

    GPIO_SetBits(GPIOD, GPIO_Pin_15);//zapalenie niebieskiej diody

        fresult = f_opendir(&Dir, "Music");

        if(fresult != FR_OK)
          return(fresult);

        for(;;)
        {
          fresult = f_readdir(&Dir, &plikInfo);

          if(fresult != FR_OK)
            return(fresult);
          if(!plikInfo.fname[0])
            break;

          if(list_pom==0)
          {
        	  first=last=add_last(last,plikInfo);
        	  list_pom++;
          }
          else
        	  last=add_last(last,plikInfo);

        }
        last->next=first;
        GPIO_SetBits(GPIOD, GPIO_Pin_14);
    for(;;)
    {

    }

	return 0;
}

void SysTick_Handler()
{
	disk_timerproc();
}
