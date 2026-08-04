################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/audio_stream_dsp/audio_stream.c \
../Core/Src/audio_stream_dsp/audio_stream_fft.c \
../Core/Src/audio_stream_dsp/audio_stream_tone.c \
../Core/Src/audio_stream_dsp/audio_stream_window.c 

OBJS += \
./Core/Src/audio_stream_dsp/audio_stream.o \
./Core/Src/audio_stream_dsp/audio_stream_fft.o \
./Core/Src/audio_stream_dsp/audio_stream_tone.o \
./Core/Src/audio_stream_dsp/audio_stream_window.o 

C_DEPS += \
./Core/Src/audio_stream_dsp/audio_stream.d \
./Core/Src/audio_stream_dsp/audio_stream_fft.d \
./Core/Src/audio_stream_dsp/audio_stream_tone.d \
./Core/Src/audio_stream_dsp/audio_stream_window.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/audio_stream_dsp/%.o Core/Src/audio_stream_dsp/%.su Core/Src/audio_stream_dsp/%.cyclo: ../Core/Src/audio_stream_dsp/%.c Core/Src/audio_stream_dsp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DSTM32F769xx -DUSE_HAL_DRIVER -D__FPU_PRESENT -DUSE_ARM_MATH -DARM_MATH_CM7 -c -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/disco_bsp" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Inc" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/ui_files" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/lvgl" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/hal_stm_lvgl/tft" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/hal_stm_lvgl/touchpad" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/Components/Fonts" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/FatFs-main/src" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/FatFs-main/ffsystem" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/Components/wm8994" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/CMSIS/DSP/Include" -I"/Users/oliverfoster/Documents/Development/Projects/carrion 2/audio_final_boss/Drivers/Log" -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -mno-unaligned-access -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src-2f-audio_stream_dsp

clean-Core-2f-Src-2f-audio_stream_dsp:
	-$(RM) ./Core/Src/audio_stream_dsp/audio_stream.cyclo ./Core/Src/audio_stream_dsp/audio_stream.d ./Core/Src/audio_stream_dsp/audio_stream.o ./Core/Src/audio_stream_dsp/audio_stream.su ./Core/Src/audio_stream_dsp/audio_stream_fft.cyclo ./Core/Src/audio_stream_dsp/audio_stream_fft.d ./Core/Src/audio_stream_dsp/audio_stream_fft.o ./Core/Src/audio_stream_dsp/audio_stream_fft.su ./Core/Src/audio_stream_dsp/audio_stream_tone.cyclo ./Core/Src/audio_stream_dsp/audio_stream_tone.d ./Core/Src/audio_stream_dsp/audio_stream_tone.o ./Core/Src/audio_stream_dsp/audio_stream_tone.su ./Core/Src/audio_stream_dsp/audio_stream_window.cyclo ./Core/Src/audio_stream_dsp/audio_stream_window.d ./Core/Src/audio_stream_dsp/audio_stream_window.o ./Core/Src/audio_stream_dsp/audio_stream_window.su

.PHONY: clean-Core-2f-Src-2f-audio_stream_dsp

