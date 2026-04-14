################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/final_boss_drivers/final_boss_audio.c \
../Drivers/final_boss_drivers/stm32f769i_eval.c 

OBJS += \
./Drivers/final_boss_drivers/final_boss_audio.o \
./Drivers/final_boss_drivers/stm32f769i_eval.o 

C_DEPS += \
./Drivers/final_boss_drivers/final_boss_audio.d \
./Drivers/final_boss_drivers/stm32f769i_eval.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/final_boss_drivers/%.o Drivers/final_boss_drivers/%.su Drivers/final_boss_drivers/%.cyclo: ../Drivers/final_boss_drivers/%.c Drivers/final_boss_drivers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DSTM32F769xx -DUSE_HAL_DRIVER -D__FPU_PRESENT -DUSE_ARM_MATH -DARM_MATH_CM7 -c -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Inc" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/final_boss_drivers" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/CMSIS/DSP/Include" -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-final_boss_drivers

clean-Drivers-2f-final_boss_drivers:
	-$(RM) ./Drivers/final_boss_drivers/final_boss_audio.cyclo ./Drivers/final_boss_drivers/final_boss_audio.d ./Drivers/final_boss_drivers/final_boss_audio.o ./Drivers/final_boss_drivers/final_boss_audio.su ./Drivers/final_boss_drivers/stm32f769i_eval.cyclo ./Drivers/final_boss_drivers/stm32f769i_eval.d ./Drivers/final_boss_drivers/stm32f769i_eval.o ./Drivers/final_boss_drivers/stm32f769i_eval.su

.PHONY: clean-Drivers-2f-final_boss_drivers

