/*
 * fpu.c
 *
 *  Created on: 2012-05-04
 *      Author: Grzesiek
 */

#include "stm32f4xx.h"


void fpu_enable( void )
{
	SCB->CPACR |= (( 3UL << 10 * 2 ) | ( 3UL << 11 * 2 ));
}
