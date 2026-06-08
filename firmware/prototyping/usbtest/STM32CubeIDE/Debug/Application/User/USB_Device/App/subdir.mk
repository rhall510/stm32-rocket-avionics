################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usb_device.c \
C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usbd_cdc_if.c \
C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usbd_desc.c 

OBJS += \
./Application/User/USB_Device/App/usb_device.o \
./Application/User/USB_Device/App/usbd_cdc_if.o \
./Application/User/USB_Device/App/usbd_desc.o 

C_DEPS += \
./Application/User/USB_Device/App/usb_device.d \
./Application/User/USB_Device/App/usbd_cdc_if.d \
./Application/User/USB_Device/App/usbd_desc.d 


# Each subdirectory must supply rules for building sources it contributes
Application/User/USB_Device/App/usb_device.o: C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usb_device.c Application/User/USB_Device/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../../Core/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../../Drivers/CMSIS/Include -I../../USB_Device/App -I../../USB_Device/Target -I../../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Application/User/USB_Device/App/usbd_cdc_if.o: C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usbd_cdc_if.c Application/User/USB_Device/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../../Core/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../../Drivers/CMSIS/Include -I../../USB_Device/App -I../../USB_Device/Target -I../../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
Application/User/USB_Device/App/usbd_desc.o: C:/Dev/Avionics/firmware/prototyping/usbtest/USB_Device/App/usbd_desc.c Application/User/USB_Device/App/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../../Core/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc -I../../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../../Drivers/CMSIS/Include -I../../USB_Device/App -I../../USB_Device/Target -I../../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Application-2f-User-2f-USB_Device-2f-App

clean-Application-2f-User-2f-USB_Device-2f-App:
	-$(RM) ./Application/User/USB_Device/App/usb_device.cyclo ./Application/User/USB_Device/App/usb_device.d ./Application/User/USB_Device/App/usb_device.o ./Application/User/USB_Device/App/usb_device.su ./Application/User/USB_Device/App/usbd_cdc_if.cyclo ./Application/User/USB_Device/App/usbd_cdc_if.d ./Application/User/USB_Device/App/usbd_cdc_if.o ./Application/User/USB_Device/App/usbd_cdc_if.su ./Application/User/USB_Device/App/usbd_desc.cyclo ./Application/User/USB_Device/App/usbd_desc.d ./Application/User/USB_Device/App/usbd_desc.o ./Application/User/USB_Device/App/usbd_desc.su

.PHONY: clean-Application-2f-User-2f-USB_Device-2f-App

