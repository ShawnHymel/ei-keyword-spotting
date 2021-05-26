################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
S_SRCS += \
../Core/Startup/startup_stm32f405rgtx.s 

S_DEPS += \
./Core/Startup/startup_stm32f405rgtx.d 

OBJS += \
./Core/Startup/startup_stm32f405rgtx.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Startup/startup_stm32f405rgtx.o: ../Core/Startup/startup_stm32f405rgtx.s Core/Startup/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -c -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/third_party/flatbuffers/include" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/third_party/gemmlowp" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/third_party/ruy" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/CMSIS/DSP/Include" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/CMSIS/DSP/PrivateInclude" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/classifier" -I"C:/Users/sgmustadio/STM32CubeIDE/workspace_1.6.0/adafruit-f405-keyword-spotting/ei-keyword-spotting/edge-impulse-sdk/CMSIS/NN/Include" -x assembler-with-cpp -MMD -MP -MF"Core/Startup/startup_stm32f405rgtx.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"

