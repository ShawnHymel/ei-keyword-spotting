# Arduino Nano 33 BLE Sense Keyword Spotting Demo

## Run Demo in Arduino

In the Arduino editor, select **Sketch > Include Library > Add .ZIP Library...** Select your downloaded .ZIP library from Edge Impulse (or select the .zip file in this directory if you would like a pre-trained demo).

**Note:** if you are updating a previous library of the same name, you may need to remove the first library from your Arduino *libraries* directory (e.g. in Windows, this is *Documents/Arduino/libraries*). Reload the Arduino editor after removing a library.

Open **File > Examples > {PROJECT_NAME} - Edge Impulse > nano_ble33_sense_microphone_continuous**.

Select the *Arduino Nano 33 BLE Sense* as your board and associated serial port. Click the **Upload** button.

If you are on Windows, you may run into the following error:

```
fork/exec C:\Users\MYUSER\AppData\Local\Arduino15\packages\arduino\tools\arm-none-eabi-gcc\7-2017q4/bin/arm-none-eabi-g++.exe: The filename or extension is too long.
```

The nested libraries may create paths that are too long for Windows. See [this article](https://docs.edgeimpulse.com/docs/running-your-impulse-arduino#code-compiling-fails-under-windows-os) for the fix.

The Arduino Nano 33 BLE Sense has a habit of resetting the serial port after uploading code, so you may need reselect the correct one once it's done.



## Licenses

See individual source code files for their respective licenses. STM32 code, Edge Impulse libraries, and TensorFlow Lite are all licensed differently.

This tutorial is licensed under **Creative Commons, Attribution 4.0 International (CC BY 4.0)**

[![License: CC BY 4.0](https://licensebuttons.net/l/by/4.0/80x15.png)](https://creativecommons.org/licenses/by/4.0/)
