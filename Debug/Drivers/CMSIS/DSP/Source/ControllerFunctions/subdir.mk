################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.c \
../Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.c 

OBJS += \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.o \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.o 

C_DEPS += \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.d \
./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/CMSIS/DSP/Source/ControllerFunctions/%.o Drivers/CMSIS/DSP/Source/ControllerFunctions/%.su Drivers/CMSIS/DSP/Source/ControllerFunctions/%.cyclo: ../Drivers/CMSIS/DSP/Source/ControllerFunctions/%.c Drivers/CMSIS/DSP/Source/ControllerFunctions/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DDEBUG -DSTM32F769xx -DUSE_HAL_DRIVER -D__FPU_PRESENT -DUSE_ARM_MATH -DARM_MATH_CM7 -c -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Inc" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Log" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Fonts" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/FatFs-main/src" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/FatFs-main/ffsystem" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/wm8994" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/Components/Common" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/final_boss_drivers" -I"/Users/oliverfoster/Documents/carrion/audio_final_boss/Drivers/CMSIS/DSP/Include" -I../Drivers/STM32F7xx_HAL_Driver/Inc -I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F7xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-CMSIS-2f-DSP-2f-Source-2f-ControllerFunctions

clean-Drivers-2f-CMSIS-2f-DSP-2f-Source-2f-ControllerFunctions:
	-$(RM) ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_f32.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q15.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_init_q31.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_f32.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q15.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_pid_reset_q31.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_f32.su ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.cyclo ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.d ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.o ./Drivers/CMSIS/DSP/Source/ControllerFunctions/arm_sin_cos_q31.su

.PHONY: clean-Drivers-2f-CMSIS-2f-DSP-2f-Source-2f-ControllerFunctions

