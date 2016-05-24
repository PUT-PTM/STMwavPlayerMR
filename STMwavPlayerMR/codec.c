//*************************************
//  codec related functions
//	http://www.mind-dump.net/configuring-the-stm32f4-discovery-for-audio
//*************************************

#include "codec.h"
__IO uint32_t  CODECTimeout = CODEC_LONG_TIMEOUT;
void codec_init()
{
	GPIO_InitTypeDef PinInitStruct;
	GPIO_StructInit(&PinInitStruct);

	I2S_InitTypeDef I2S_InitType;
	I2C_InitTypeDef I2C_InitType;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA| RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOC|RCC_AHB1Periph_GPIOD, ENABLE);

	//Reset pin as GPIO
	PinInitStruct.GPIO_Pin = CODEC_RESET_PIN;
	PinInitStruct.GPIO_Mode = GPIO_Mode_OUT;
	PinInitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
	PinInitStruct.GPIO_OType = GPIO_OType_PP;
	PinInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &PinInitStruct);

	// I2C pins
	PinInitStruct.GPIO_Mode = GPIO_Mode_AF;
	PinInitStruct.GPIO_OType = GPIO_OType_OD;
	PinInitStruct.GPIO_Pin = I2C_SCL_PIN | I2C_SDA_PIN;
	PinInitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
	PinInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &PinInitStruct);

	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_I2C1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource9, GPIO_AF_I2C1);

	//enable I2S and I2C clocks
	//RCC_I2SCLKConfig(RCC_I2S2CLKSource_PLLI2S);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 | RCC_APB1Periph_SPI3, ENABLE);
	RCC_PLLI2SCmd(ENABLE);

	// I2S pins
	PinInitStruct.GPIO_OType = GPIO_OType_PP;
	PinInitStruct.GPIO_Pin = I2S3_SCLK_PIN | I2S3_SD_PIN | I2S3_MCLK_PIN;
	GPIO_Init(GPIOC, &PinInitStruct);

	PinInitStruct.GPIO_Pin = I2S3_WS_PIN;
	GPIO_Init(GPIOA, &PinInitStruct);

	//prepare output ports for alternate function
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource4, GPIO_AF_SPI3);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_SPI3);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource10, GPIO_AF_SPI3);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource12, GPIO_AF_SPI3);

	//keep Codec off for now
	GPIO_ResetBits(GPIOD, CODEC_RESET_PIN);

	// configure I2S port
	SPI_I2S_DeInit(CODEC_I2S);
	I2S_InitType.I2S_AudioFreq = I2S_AudioFreq_44k;
	I2S_InitType.I2S_MCLKOutput = I2S_MCLKOutput_Enable;
	I2S_InitType.I2S_DataFormat = I2S_DataFormat_16b;
	I2S_InitType.I2S_Mode = I2S_Mode_MasterTx;
	I2S_InitType.I2S_Standard = I2S_Standard_Phillips;
	I2S_InitType.I2S_CPOL = I2S_CPOL_Low;

	I2S_Init(CODEC_I2S, &I2S_InitType);
	//I2S_Cmd(CODEC_I2S, ENABLE);

	// configure I2C port
	I2C_DeInit(CODEC_I2C);
	I2C_InitType.I2C_ClockSpeed = 100000;
	I2C_InitType.I2C_Mode = I2C_Mode_I2C;
	I2C_InitType.I2C_OwnAddress1 = CORE_I2C_ADDRESS;
	I2C_InitType.I2C_Ack = I2C_Ack_Enable;
	I2C_InitType.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitType.I2C_DutyCycle = I2C_DutyCycle_2;

	I2C_Cmd(CODEC_I2C, ENABLE);
	I2C_Init(CODEC_I2C, &I2C_InitType);
}

void codec_ctrl_init()
{
	uint32_t delaycount;
	uint8_t CodecCommandBuffer[5];

	uint8_t regValue = 0xFF;

	GPIO_SetBits(GPIOD, CODEC_RESET_PIN);
	delaycount = 1000000;
	while (delaycount > 0)
	{
		delaycount--;
	}
	//keep codec OFF
	CodecCommandBuffer[0] = CODEC_MAP_PLAYBACK_CTRL1;
	CodecCommandBuffer[1] = 0x01;
	send_codec_ctrl(CodecCommandBuffer, 2);

	//begin initialization sequence (p. 32)
	CodecCommandBuffer[0] = 0x00;
	CodecCommandBuffer[1] = 0x99;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = 0x47;
	CodecCommandBuffer[1] = 0x80;
	send_codec_ctrl(CodecCommandBuffer, 2);

	regValue = read_codec_register(0x32);

	CodecCommandBuffer[0] = 0x32;
	CodecCommandBuffer[1] = regValue | 0x80;
	send_codec_ctrl(CodecCommandBuffer, 2);

	regValue = read_codec_register(0x32);

	CodecCommandBuffer[0] = 0x32;
	CodecCommandBuffer[1] = regValue & (~0x80);
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = 0x00;
	CodecCommandBuffer[1] = 0x00;
	send_codec_ctrl(CodecCommandBuffer, 2);
	//end of initialization sequence

	CodecCommandBuffer[0] = CODEC_MAP_PWR_CTRL2;
	CodecCommandBuffer[1] = 0xAF;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = CODEC_MAP_PLAYBACK_CTRL1;
	CodecCommandBuffer[1] = 0x70;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = CODEC_MAP_CLK_CTRL;
	CodecCommandBuffer[1] = 0x81; //auto detect clock
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = CODEC_MAP_IF_CTRL1;
	CodecCommandBuffer[1] = 0x07;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = 0x0A;
	CodecCommandBuffer[1] = 0x00;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = 0x27;
	CodecCommandBuffer[1] = 0x00;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = 0x1A | CODEC_MAPBYTE_INC;
	CodecCommandBuffer[1] = 0x0A;
	CodecCommandBuffer[2] = 0x0A;
	send_codec_ctrl(CodecCommandBuffer, 3);

	CodecCommandBuffer[0] = 0x1F;
	CodecCommandBuffer[1] = 0x0F;
	send_codec_ctrl(CodecCommandBuffer, 2);

	CodecCommandBuffer[0] = CODEC_MAP_PWR_CTRL1;
	CodecCommandBuffer[1] = 0x9E;
	send_codec_ctrl(CodecCommandBuffer, 2);

}

void send_codec_ctrl(uint8_t controlBytes[], uint8_t numBytes)
{
	uint8_t bytesSent=0;

	while (I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BUSY))
	{
		//just wait until no longer busy
	}

	I2C_GenerateSTART(CODEC_I2C, ENABLE);
	while (!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_SB))
	{
		//wait for generation of start condition
	}
	I2C_Send7bitAddress(CODEC_I2C, CODEC_I2C_ADDRESS, I2C_Direction_Transmitter);
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		//wait for end of address transmission
	}
	while (bytesSent < numBytes)
	{
		I2C_SendData(CODEC_I2C, controlBytes[bytesSent]);
		bytesSent++;
		while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
		{
			//wait for transmission of byte
		}
	}
	while(!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BTF))
	{
	    //wait until it's finished sending before creating STOP
	}
	I2C_GenerateSTOP(CODEC_I2C, ENABLE);

}

uint8_t read_codec_register(uint8_t mapbyte)
{
	uint8_t receivedByte = 0;

	while (I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BUSY))
	{
		//just wait until no longer busy
	}

	I2C_GenerateSTART(CODEC_I2C, ENABLE);
	while (!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_SB))
	{
		//wait for generation of start condition
	}

	I2C_Send7bitAddress(CODEC_I2C, CODEC_I2C_ADDRESS, I2C_Direction_Transmitter);
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
	{
		//wait for end of address transmission
	}

	I2C_SendData(CODEC_I2C, mapbyte); //sets the transmitter address
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
	{
		//wait for transmission of byte
	}

	I2C_GenerateSTOP(CODEC_I2C, ENABLE);

	while (I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BUSY))
	{
		//just wait until no longer busy
	}

	I2C_AcknowledgeConfig(CODEC_I2C, DISABLE);

	I2C_GenerateSTART(CODEC_I2C, ENABLE);
	while (!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_SB))
	{
		//wait for generation of start condition
	}

	I2C_Send7bitAddress(CODEC_I2C, CODEC_I2C_ADDRESS, I2C_Direction_Receiver);
	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
		//wait for end of address transmission
	}

	while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED))
	{
		//wait until byte arrived
	}
	receivedByte = I2C_ReceiveData(CODEC_I2C);

	I2C_GenerateSTOP(CODEC_I2C, ENABLE);

	return receivedByte;
}
void Codec_VolumeCtrl(uint8_t Volume)
{

  if (Volume > 0xE6)
  {
    /* Set the Master volume */
    Codec_WriteRegister(0x20, Volume - 0xE7);
    Codec_WriteRegister(0x21, Volume - 0xE7);
  }
  else
  {
    /* Set the Master volume */
    Codec_WriteRegister(0x20, Volume + 0x19);
    Codec_WriteRegister(0x21, Volume + 0x19);
  }
}

/**
  * @brief Writes a Byte to a given register into the audio codec through the
           control interface (I2C)
  * @param RegisterAddr: The address (location) of the register to be written.
  * @param RegisterValue: the Byte value to be written into destination register.
  * @retval o if correct communication, else wrong communication
  */
void Codec_WriteRegister(uint32_t RegisterAddr, uint32_t RegisterValue)
{
  uint32_t result = 0;

  /*!< While the bus is busy */
  CODECTimeout = CODEC_LONG_TIMEOUT;
  while(I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BUSY))
  {
    if((CODECTimeout--) == 0) return Codec_TIMEOUT_UserCallback();
  }

  /* Start the config sequence */
  I2C_GenerateSTART(CODEC_I2C, ENABLE);

  /* Test on EV5 and clear it */
  CODECTimeout = CODEC_FLAG_TIMEOUT;
  while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((CODECTimeout--) == 0) return Codec_TIMEOUT_UserCallback();
  }

  /* Transmit the slave address and enable writing operation */
  I2C_Send7bitAddress(CODEC_I2C, CODEC_I2C_ADDRESS, I2C_Direction_Transmitter);

  /* Test on EV6 and clear it */
  CODECTimeout = CODEC_FLAG_TIMEOUT;
  while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
  {
    if((CODECTimeout--) == 0) return Codec_TIMEOUT_UserCallback();
  }

  /* Transmit the first address for write operation */
  I2C_SendData(CODEC_I2C, RegisterAddr);

  /* Test on EV8 and clear it */
  CODECTimeout = CODEC_FLAG_TIMEOUT;
  while (!I2C_CheckEvent(CODEC_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTING))
  {
    if((CODECTimeout--) == 0) return Codec_TIMEOUT_UserCallback();
  }

  /* Disable the interrupts mechanism to prevent the I2C communication from corruption */
  __disable_irq();

  /* Prepare the register value to be sent */
  I2C_SendData(CODEC_I2C, RegisterValue);

  /*!< Wait till all data have been physically transferred on the bus */
  CODECTimeout = CODEC_LONG_TIMEOUT;
  while(!I2C_GetFlagStatus(CODEC_I2C, I2C_FLAG_BTF))
  {
    if((CODECTimeout--) == 0) Codec_TIMEOUT_UserCallback();
  }

  /* End the configuration sequence */
  I2C_GenerateSTOP(CODEC_I2C, ENABLE);

#ifdef VERIFY_WRITTENDATA
  /* Verify that the data has been correctly written */
  result = (Codec_ReadRegister(RegisterAddr) == RegisterValue)? 0:1;
#endif /* VERIFY_WRITTENDATA */

  /* Re-enable the interrupt mechanism */
  __enable_irq();

  /* Return the verifying value: 0 (Passed) or 1 (Failed) */
  return result;
}

void Codec_TIMEOUT_UserCallback(void)
{
  /* Block communication and all processes */
  while (1)
  {
  }
}

void Codec_PauseResume(uint32_t Cmd)
{
  /* Pause the audio file playing */
  if (Cmd == AUDIO_PAUSE)
  {
    /* Mute the output first */
    Codec_Mute(AUDIO_MUTE_ON);

    /* Put the Codec in Power save mode */
    Codec_WriteRegister(0x02, 0x01);
  }
  else /* AUDIO_RESUME */
  {
    /* Unmute the output first */
    Codec_Mute(AUDIO_MUTE_OFF);

    /* Exit the Power save mode */
    Codec_WriteRegister(0x02, 0x9E);
  }
}

void Codec_Mute(uint32_t Cmd)
{
  /* Set the Mute mode */
  if (Cmd == AUDIO_MUTE_ON)
  {
    Codec_WriteRegister(0x04, 0xFF);
  }
  else /* AUDIO_MUTE_OFF Disable the Mute */
  {
    Codec_WriteRegister(0x04, 0);
  }
}
