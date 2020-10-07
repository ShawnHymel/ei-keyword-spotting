# STM32 Nucleo-L476RG Keyword Spotting Demo

## Hardware Connections

| MCU Pin | MCU Function | Nucleo Pin | Mic Board Pin |
|:-------:|:------------:|:----------:|:-------------:|
|         |              |    3.3V    |       3V      |
|         |              |     GND    |      GND      |
|   PB3   |  SAI1_SCK_B  |      D3    |     BCLK      |
|   PB5   |   SAI1_SD_B  |      D4    |     DOUT      |
|   PA4   |   SAI1_FS_B  |      A2    |     LRCL      |
|    -    |       -      |       -    |      SEL      |

## Run Demo with Custom Model in STM32CubeIDE

Select **File > Open Projects from File System...** and click **Directory...**. Select your demo project folder (e.g. *nucleo-l476-keyword-spotting*) from the downloaded repository. Click **Finish**.

%%%screen-stm32-import-dir

In the *Project Explorer*, delete the **model-parameters** and **tflite-model** directories in the *ei-keyword-spotting* diretory of your project, as these contain a previously trained model on the "yes" and "no" keywords.

Select **File > Import**. For the *From directory* section, select the **model-parameters** directory is selected from your downloaded Edge Impulse library. Select the **model-parameters** folder in the left pane to include the header files in the import. Select **<project_name>/ei-keyword-spotting** as the *Into folder*. Select **Create top-level folder**. Click **Finish**.

%%%importing dir

Repeat this process for the **tflite-model** directory in the downloaded Edge Impulse library.

At this point, you should have replaced the *model-parameters* and *tflite-model* directories in your demo project. Open **<project_name>/Core/Src/main.cpp** to edit the program's `main()` function. Feel free to look through the code to see how STM32 sets up its peripherals and how the Edge Impulse library is initialized and used. Find the `// YOUR CODE GOES HERE!` section to do something with the inference results.

%%%stm32 project

Select **Project > Build Configurations > Set Active > Release**. Then, select **Project > Build Project**. Wait while the project builds.

Select **Run > Run Configurations**. Click the **New Configuration** button (top left) to create a new configuration profile. It should be named "<project_name> Release" and use the *Release/<project_name>.elf* file for the application. If not, click **Search Project...** and select the *Release* .elf file for your project. Leave everything else as default. Click **Run**.

%%%stm32 run config

Use a serial terminal program (such as [PuTTY](https://www.putty.org/)) to view the output of your keyword spotting system.

![Running keyword spotting demo on STM32](https://raw.githubusercontent.com/ShawnHymel/ei-keyword-spotting/master/images/screen-serial-output.png)

## Licenses

See individual source code files for their respective licenses. STM32 code, Edge Impulse libraries, and TensorFlow Lite are all licensed differently.

This tutorial is licensed under **Creative Commons, Attribution 4.0 International (CC BY 4.0)**

[![License: CC BY 4.0](https://licensebuttons.net/l/by/4.0/80x15.png)](https://creativecommons.org/licenses/by/4.0/)