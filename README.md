# FanGuardian Controller
_FanGuardian Controller_ is the extension board of _FanGuardian_, an innovative and revolutionary piece of hardware designed to elevate the user experience in PC building and customization. 

# Getting Started
The _FanGuardian Controller_ firmware is written in C++. If you want to do any modification, we suggest to use [Microsoft Visual Studio Code](https://code.visualstudio.com/) with PlatformIO extension. For flashing bootloader, we recommend to use [STLinkV2](https://www.st.com/en/development-tools/st-link-v2.html) with [OpenOCD](https://openocd.org/pages/getting-openocd.html)

## Features
The board can measure the +12V, +5V and +3V3 rail voltage levels from a PC motherboard or power supply.

In addition it can measure two individual temperature values with an integrated 10kOhm pull-up resistor, and two individual temperature values without pull-ups. Every type of 10k NTC thermistor is a good choice for the first one for example: [link](https://www.amazon.com/s?k=ntc+temperature+sensor+stop+fitting+bitspower&crid=3JYK7CNCWXJ96&sprefix=ntc+temperature+sensor+stop+fitting+bitspower%2Caps%2C160&ref=nb_sb_noss). The second one is designed to operate with [Singularity Computers Powerboards](https://singularitycomputers.com/product-category/powerboard/powerboard-powerboard/) as the PowerBoard has integrated pull-up to the power rail.

These two type of measurements are transferred via I2C bus.

Also there is an option to measure four PC fan RPMs (only those that have open drain/open-collector output from Hall-effect sensor). This is done by the Atmel® SMART SAM D21 microcontroller onboard, which sends the results through the UART port.
All these measurements are sent to _FanGuardian_ that processes and displays the results on the UI. For the details check out that repo [here](https://github.com/SingularityComputers/FanGuardianUI).

## Build and flashing
### Flashing the bootloader
_FanGuardian Controller_ is using the popular [Seeeduino XIAO M0 bootloader](https://github.com/Seeed-Studio/ArduinoCore-samd/tree/master/bootloaders/XIAOM0) because its simplicity. You can replace it to your own any time you want. For that, there is JTAG SWD port on the board. Just attach your STLinkV2 to the SWDIO, SWCLK, +5V and GND pins, and run `openocd -f flash.cfg` from `BootloaderTool_STLink` folder. Do not forget to change the `flash.cfg` depending on your needs. If everything was successful, you should see something like:
```
Info : clock speed 400 kHz
Info : STLINK V2J28S7 (API v2) VID:PID 0483:3748
Info : Target voltage: 3.177202
Info : [at91samd21g18.cpu] Cortex-M0+ r0p1 processor detected
Info : [at91samd21g18.cpu] target has 4 breakpoints, 2 watchpoints
Info : starting gdb server for at91samd21g18.cpu on 3333
Info : Listening on port 3333 for gdb connections
[at91samd21g18.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x61000000 pc: 0x00000288 msp: 0x20002dd8
[at91samd21g18.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x81000000 pc: 0x00000288 msp: 0x20002dd8
** Programming Started **
Info : SAMD MCU: SAMD21G18A (256KB Flash, 32KB RAM)
** Programming Finished **
** Verify Started **
** Verified OK **
[at91samd21g18.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x21000000 pc: 0x00000288 msp: 0x20002dd8
** Programming Started **
** Programming Finished **
** Verify Started **
** Verified OK **
```
### Uploading firmware
The firmware can be uploaded with OpenOCD similar to the bootloader, as it described in the previous section. However, the simplest way is to use Visual Studio Code with PlatformIO extension, and upload the code directly to the board using its USB port.

## Further development
As the Atmel® SMART SAM D21 has lot of extra pins that are traced out to the board (PB10, PB11, PA12, PA13 on JP5 and PB8, PB9, PA2, PA4, PA5, PA6, PA7 on JP8), you can design and develop any additional extensions easily.

## License
As we are committed to our customers and to the PC modding community, we have decided to opensource our firmwares to encourage and inspire the community to create something new and better together.

This means that the code is licensed under:
- [GNU General Public License v3.0](https://choosealicense.com/licenses/gpl-3.0/).

The GNU GPLv3 lets you to do almost anything you want with your project, except distributing closed source versions.




