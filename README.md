# Enlighted-Otter

Enlighted-Otter is an Open-Source and OSHW (Design files still missing, will upload later)  work-light for hacker/maker events like the chaos communication congress. It is based upon a STM32F334 with its 
high resolution timer as dual boost 
converter. The main goal is to provide cableless, high CRI, high brightness, flicker-free illumination with a variable color temperature. This is achieved by using LEDs with high CRI (>93, STW9Q14C) and 
the boost 
converter operating at a frequency of up to 850khz. The PCB also features USB-C (power profile 1) to deliver enough power to charge the 18650 type batteries as well as keep up operation of the boost converter.

Enlighted-Otter can be screwed onto an empty bottle of Mate (or similar), therefore serving with a very small footprint.

## Building and flashing

Change *Inc/defines.h* to fit your desires, then build with

`make clean all`

and flash it via Ozone or st-utils

`st-flash --reset write build/*.bin 0x8000000`

or OpenOCD

`openocd -f interface/jlink.cfg -f board/stm32f334discovery.cfg -c "init reset init program otter.bin reset run exit 0x08000000" -c shutdown` (not tested)


## Images

![Battery 1](https://raw.githubusercontent.com/Jan--Henrik/Enlighted-Otter/master/Images/final_tisch.jpeg)
![Battery 2](https://raw.githubusercontent.com/Jan--Henrik/Enlighted-Otter/master/Images/final_werkstatt.jpeg)

![Top View](https://raw.githubusercontent.com/Jan--Henrik/Enlighted-Otter/master/Images/Enlighted_Otter_1.jpeg)
![Side View](https://raw.githubusercontent.com/Jan--Henrik/Enlighted-Otter/master/Images/Enlighted_Otter_2.jpeg)
![Bottom View](https://raw.githubusercontent.com/Jan--Henrik/Enlighted-Otter/master/Images/Enlighted_Otter_3.jpeg)

## Videos

#### Boost converter operation

[![Boost 1](https://img.youtube.com/vi/A-QjU9mWTO4/0.jpg)](https://youtu.be/A-QjU9mWTO4)

#### TSC operation

[![TSC 1](https://img.youtube.com/vi/ADD4yiM9S0Q/0.jpg)](https://youtu.be/ADD4yiM9S0Q)

#### LED brightness test

[![LED 1](https://img.youtube.com/vi/DC_eAY72nbw/0.jpg)](https://youtu.be/DC_eAY72nbw)

## Known Bugs

- Touch does not work when licked (tested by @NiklasFauth)

## Planning

### Current state:

HW is flashable, both bosst converter work properly up to a current of ~350mA, current regulation works on both boost converters (+-0.5-1.5mA), RT9466 seems to do its job, does ~not~ work properly on 
batteries ~(5V gate driver power supply is missing, need to fix OTG)~, LED outputs and Touch inputs work, ~FLT_1 is always on~
Advanced User interface is also working, current and color can be set via touch input. Soft on/off works fine. Gamma correcture is applied

HW regulates after boot while main loop is basically empty \o/

### To do:

- [ ] Overtemp protection
- [ ] "Mobile handling"
	- [ ] Touch is not that responsive
	- [ ] Battery low alarm/turnoff
- [ ] Test I2C - broken?
- [ ] Rewrite regulation, seems to be flickering
- [ ] Make user Interface more responsive
- [ ] HW
	-  See HW/README.md
- [ ] Order new PCB
- [ ] find MPP
- [ ] Make it more efficient
- [ ] Find more to do's

### Done:

- [x] 3D Design
- [x] get rid of flicker at 0.5-2.5mA 
- [x] temperature calculation
- [x] get rid of global variables
	- moved them into structs
- [x] make the code more Timer/Interrupt based
	- [x] HRTIM
	- [x] HRTIM FLT
	- [x] ADC
	- [x] TSC
	- [x] I2C/UART
	- [x] Regulator
	- [x] UI
- [x] gamma correction
	- [x] write tool to generate gamma correction curve
	- [x] Implement it
	- [x] Test/debug it
- [x] Fix the HRTIM FLT1 line
- [x] Test Injected ADC
	- [x] ADC tested and seems to work just fine
	- [x] Add moving average filter to smooth out ADC values
	- [x] Add JEOC Interrupt
- [x] current calculation
- [x] Fix/write TSC controller
	- [x] Add second TSC bank
	- [x] Make it interrupt based
		- [x] write code
		- [x] Test that
- [x] Fix I2C timing/Interrupt issue
- [x] Get HRTIM to run
- [x] Get the LED's to light up
- [x] Write regulator
	- [x] Write simple, working I regulator
	- [x] Make it cycle time independent
	- [x] Make regulator interrupt based
	- [x] Write a better PI regulator
- [x] Basic TSC,UART,DAC,COMP,I2C and GPIO functionality
- [x] Write the user interface
	- [x] Make it usable
- [x] Readout of touchslider
- [x] Be an otter
- [x] PCB does not burn
