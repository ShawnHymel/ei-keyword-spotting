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

Open the **Serial Monitor**, and you should see the predictions being printed to the screen. Try saying one of your keywords. The prediction output (essentially, the probability that the model thinks it heard that word/phrase) should go up.

![Arduino running Edge Impulse continuous keyword spotting library](https://raw.githubusercontent.com/ShawnHymel/ei-keyword-spotting/master/images/screen-arduino-serial-monitor.png)

## Modifying the Demo Program

I've added a couple of comments in the screenshot below to show you where you should add your own code. In the example, I look for the *forward* class (index of 2--refer to the order of appearance in the Serial Monitor) to produce a predicted probability of 0.7 or above. If that happens, I flash the Arduino Nano's onboard LED (don't forget to add `pinMode(LED_BUILTIN, OUTPUT);` in `setup()`).

![Adding custom code to the Arduino keyword spotting demo](https://raw.githubusercontent.com/ShawnHymel/ei-keyword-spotting/master/images/screen-arduino-custom-code.png)

## Licenses

See individual source code files for their respective licenses. STM32 code, Edge Impulse libraries, and TensorFlow Lite are all licensed differently.

This tutorial is licensed under **Creative Commons, Attribution 4.0 International (CC BY 4.0)**

[![License: CC BY 4.0](https://licensebuttons.net/l/by/4.0/80x15.png)](https://creativecommons.org/licenses/by/4.0/)
