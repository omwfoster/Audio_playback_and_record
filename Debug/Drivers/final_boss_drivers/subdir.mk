################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/final_boss_drivers/stm32f769i_discovery.c \
../Drivers/final_boss_drivers/stm32f769i_discovery_audio.c \
../Drivers/final_boss_drivers/stm32f769i_discovery_lcd.c \
../Drivers/final_boss_drivers/stm32f769i_discovery_sd.c \
../Drivers/final_boss_drivers/stm32f769i_discovery_sdram.c \
../Drivers/final_boss_drivers/stm32f769i_discovery_ts.c \
../Drivers/final_boss_drivers/waveplayer.c \
../Drivers/final_boss_drivers/waverecorder.c 

OBJS += \
./Drivers/final_boss_drivers/stm32f769i_discovery.o \
./Drivers/final_boss_drivers/stm32f769i_discovery_audio.o \
./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.o \
./Drivers/final_boss_drivers/stm32f769i_discovery_sd.o \
./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.o \
./Drivers/final_boss_drivers/stm32f769i_discovery_ts.o \
./Drivers/final_boss_drivers/waveplayer.o \
./Drivers/final_boss_drivers/waverecorder.o 

C_DEPS += \
./Drivers/final_boss_drivers/stm32f769i_discovery.d \
./Drivers/final_boss_drivers/stm32f769i_discovery_audio.d \
./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.d \
./Drivers/final_boss_drivers/stm32f769i_discovery_sd.d \
./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.d \
./Drivers/final_boss_drivers/stm32f769i_discovery_ts.d \
./Drivers/final_boss_drivers/waveplayer.d \
./Drivers/final_boss_drivers/waverecorder.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/final_boss_drivers/%.o Drivers/final_boss_drivers/%.su Drivers/final_boss_drivers/%.cyclo: ../Drivers/final_boss_drivers/%.c Drivers/final_boss_drivers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DSTM32F769xx -DUSE_HAL_DRIVER -D__FPU_PRESENT -DUSE_ARM_MATH -DARM_MATH_CM7 -c -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Inc" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Fonts" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/FatFs-main/src" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/FatFs-main/ffsystem" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/wm8994" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/final_boss_drivers" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/CMSIS/DSP/Include" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Log" -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-final_boss_drivers

clean-Drivers-2f-final_boss_drivers:
	-$(RM) ./Drivers/final_boss_drivers/stm32f769i_discovery.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery.d ./Drivers/final_boss_drivers/stm32f769i_discovery.o ./Drivers/final_boss_drivers/stm32f769i_discovery.su ./Drivers/final_boss_drivers/stm32f769i_discovery_audio.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery_audio.d ./Drivers/final_boss_drivers/stm32f769i_discovery_audio.o ./Drivers/final_boss_drivers/stm32f769i_discovery_audio.su ./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.d ./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.o ./Drivers/final_boss_drivers/stm32f769i_discovery_lcd.su ./Drivers/final_boss_drivers/stm32f769i_discovery_sd.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery_sd.d ./Drivers/final_boss_drivers/stm32f769i_discovery_sd.o ./Drivers/final_boss_drivers/stm32f769i_discovery_sd.su ./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.d ./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.o ./Drivers/final_boss_drivers/stm32f769i_discovery_sdram.su ./Drivers/final_boss_drivers/stm32f769i_discovery_ts.cyclo ./Drivers/final_boss_drivers/stm32f769i_discovery_ts.d ./Drivers/final_boss_drivers/stm32f769i_discovery_ts.o ./Drivers/final_boss_drivers/stm32f769i_discovery_ts.su ./Drivers/final_boss_drivers/waveplayer.cyclo ./Drivers/final_boss_drivers/waveplayer.d ./Drivers/final_boss_drivers/waveplayer.o ./Drivers/final_boss_drivers/waveplayer.su ./Drivers/final_boss_drivers/waverecorder.cyclo ./Drivers/final_boss_drivers/waverecorder.d ./Drivers/final_boss_drivers/waverecorder.o ./Drivers/final_boss_drivers/waverecorder.su

.PHONY: clean-Drivers-2f-final_boss_drivers

