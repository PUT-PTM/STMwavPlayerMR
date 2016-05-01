#STMwavPlayerMR

##Overview
The project of simple .wav player based on STM32f4-DISCOVERY board. It plays .wav files from SD card.

##Description
The idea to play .wav files was implement the cyclic list. It contains information about files. You couldn't know the "name" of file, you only need to use the button to switch into next music file. We use SPI for sending data from SD card to STM32F4 because SD card module doesn't support SDIO communication. It uses DMA module for smoother playback of music. To control the volume of songs we use the potentiometer.

##Tools
- CooCox CoIDE, Version: 1.7.8

##How to run
To run the project you should have hardware:
- STM32f4-DISCOVERY board,
- SD Card Module and SD Card formatted to fat32,
- Headphone or loudspeaker with male jack connector.

How to use?

1. Connect STM32F4-DISCOVERY board with SD Card Module in this way:
   STM32 <---> SD Card Module
  * GND  <---> GND
  * 3V   <---> 3V3
  * PB11 <---> CS
  * PB15 <---> MOSI
  * PB13 <---> SCK
  * PB14 <---> MISO
  * GND  <---> GND
  
2. Plug your SD car with .wav files into the module.
3. Build this project with CooCox CoIDE and Download Code to Flash.

##How to compile
The only step is download the project and compile it with CooCox CoIDE.

##Future improvements
- Add volume changing by potentiometer.
- Fix errors with SD card.

##Attributions
http://elm-chan.org/fsw/ff/00index_e.html

http://www.mind-dump.net/configuring-the-stm32f4-discovery-for-audio

http://www1.coocox.org/repo/a8bde334-159f-4dae-bc65-686695a3e545/src/STM3240_41_G_EVAL/stm324xg_audio_codec.h.htm

http://www1.coocox.org/repo/a8bde334-159f-4dae-bc65-686695a3e545/src/STM3240_41_G_EVAL/stm324xg_audio_codec.c.htm

https://www.youtube.com/watch?v=EYs3f4uwYTo

##License

##Credits
* Monika Grądzka
* Robert Kazimierczak

The project was conducted during the Microprocessor Lab course held by the Institute of Control and Information Engineering, Poznan University of Technology.

Supervisor: Tomasz Mańkowski
