Description:

STM32-based Tone Generator with SD Card preset-system, LCD UI and DAC-driven sound output.

Features: 
- Preset-based waveforms (sine, square, triangle, saw)
- SD card filesystem (FatFS)
- LCD menu with buttons
- Timer-driven DAC audio engine
- Adjustable frequency and amplitude by way of preset file system

Hardware overview:

Hardware	|		Description
------------|-------------------------------------------------------------------		
MCU			|		STM32F446RE
------------|-------------------------------------------------------------------		
Sound		|		PAM8403 Amplifier + Same Sky 1W Miniature Speaker
------------|-------------------------------------------------------------------							
Storage		|		PmodMicroSD Card Slot + Transcend 4GB MicroSD Card, Class 10
------------|-------------------------------------------------------------------
UI			|		LCD1602 + 2 Buttons
--------------------------------------------------------------------------------		

Software architecture:

Application Layer:

- UI state machine
- Preset parser
- Audio engine (timer interrupt + phase accumulator)

Middleware:

- FatFS (SD card file system)

Hardware Drivers:

- SPI Driver (SD card communication)
- GPIO (Buttons)
- I2C (LCD)
- Timer (Audio sample rate)
- DAC (Audio output)

Additional hardware documentation:

- Block diagram (ref. Tone Generator Block Diagram.png)
- Wiring diagram (ref. Tone Generator Wiring Diagram.png)
- Picture of physical hardware setup (ref. Hardware Setup.png) 

		