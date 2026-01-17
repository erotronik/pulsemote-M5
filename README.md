## PulseMote-M5

A remote control using the M5-Remote for various e-Stim and other devices

## Building for the M5Stack CoreS3

This currently uses the platformio system

## Building for the M5Stack Tab5

This currently uses esp-idf system.

idf.py -p /dev/ttyACM0 build flash monitor

## If you want more than 3 Bluetooth devices at the same time

Unfortunately the C6 coprocessor is compiled to only support 3. You need to reflash it. Perhaps
you can do this via the 'OTA' method, but untested, I did it flashing via hardware.

You also need a version of esp-hosted-mcu prior to 2.5.2.  This works:

git clone https://github.com/espressif/esp-hosted-mcu.git && cd esp-hosted-mcu/
git checkout 757e90bf20cdd9655be06058e2acbd6c369f6d9d
cd slave
idf.py set-target esp32c6
idf.py menuconfig
  in "Component Config" "Bluetooth" "Controller Options" set "Maximum number of concurrent connections" to 9
if.py -p /dev/ttyUSB0 build flash


