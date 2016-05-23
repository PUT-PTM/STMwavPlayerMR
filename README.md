#STMwavPlayerMR

##Overview
The project of simple .wav player based on STM32f4-DISCOVERY board. It plays .wav files from SD card.

##Description
The idea to play .wav files was implement the two-way cyclic list. It contains information about files. You don't need to know the "name" of file, you only need to use one of external switches to switch into next or previous music file or pause/resume playing music. If you use the user button, you switch into random mode of playing .wav files. We use SPI for sending data from SD card to STM32F4 because SD card module doesn't support SDIO communication. It uses DMA module for smoother playback of music. To control the volume of songs we use the potentiometer. What is more, the simple error handling was implemented. When you spot blinking:
* green LED - cable fault, SD Card isn't inserted into SD Card Module or SD Card isn't formatted to FAT32,
* orange LED - SD Card has been removed during listening music,
* red LED - SD Card doesn't include .wav files.

##Tools
- CooCox CoIDE, Version: 1.7.8

##How to run
To run the project you should have hardware:
- STM32f4-DISCOVERY board,
- SD Card Module and SD Card formatted to FAT32,
- Headphone or loudspeaker with male jack connector,
- Rotary potentiometer - linear (10k ohm),
- 3 switches.

How to use?

1. Connect STM32F4-DISCOVERY board with SD Card Module in this way:
  * STM32 <---> SD Card Module
  * GND  <---> GND
  * 3V   <---> 3V3
  * PB11 <---> CS
  * PB15 <---> MOSI
  * PB13 <---> SCK
  * PB14 <---> MISO
  * GND  <---> GND
2. Connect also potentiometer (GND, PA1, VDD) and 3 switches (PA5, PA7, PA8).
3. Plug your SD Card with .wav files into the module.
4. Build this project with CooCox CoIDE and Download Code to Flash.
5. When you notice the fault alarmed by blinking LEDs, you should fix it and press RESET button.

##How to compile
The only step is download the project and compile it with CooCox CoIDE.

##Attributions
- http://elm-chan.org/fsw/ff/00index_e.html
- http://www.mind-dump.net/configuring-the-stm32f4-discovery-for-audio
- http://www1.coocox.org/repo/a8bde334-159f-4dae-bc65-686695a3e545/src/STM3240_41_G_EVAL/stm324xg_audio_codec.h.htm
- http://www1.coocox.org/repo/a8bde334-159f-4dae-bc65-686695a3e545/src/STM3240_41_G_EVAL/stm324xg_audio_codec.c.htm
- https://www.youtube.com/watch?v=EYs3f4uwYTo

##License
MIT

##Credits
* Monika Grądzka
* Robert Kazimierczak

The project was conducted during the Microprocessor Lab course held by the Institute of Control and Information Engineering, Poznan University of Technology.

Supervisor: Tomasz Mańkowski
