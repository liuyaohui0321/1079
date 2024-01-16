################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
LD_SRCS += \
../src/lscript.ld 

C_SRCS += \
../src/7x_dma.c \
../src/alloc.c \
../src/check.c \
../src/cmd.c \
../src/main.c \
../src/malloc.c \
../src/mem_test.c \
../src/nhc_amba.c \
../src/platform.c \
../src/simple_dma.c \
../src/sys.c \
../src/time.c \
../src/uart.c \
../src/uart_get.c \
../src/xllfifo_polling_example.c \
../src/xuartlite_intr_example.c \
../src/xuartlite_polled_example.c 

OBJS += \
./src/7x_dma.o \
./src/alloc.o \
./src/check.o \
./src/cmd.o \
./src/main.o \
./src/malloc.o \
./src/mem_test.o \
./src/nhc_amba.o \
./src/platform.o \
./src/simple_dma.o \
./src/sys.o \
./src/time.o \
./src/uart.o \
./src/uart_get.o \
./src/xllfifo_polling_example.o \
./src/xuartlite_intr_example.o \
./src/xuartlite_polled_example.o 

C_DEPS += \
./src/7x_dma.d \
./src/alloc.d \
./src/check.d \
./src/cmd.d \
./src/main.d \
./src/malloc.d \
./src/mem_test.d \
./src/nhc_amba.d \
./src/platform.d \
./src/simple_dma.d \
./src/sys.d \
./src/time.d \
./src/uart.d \
./src/uart_get.d \
./src/xllfifo_polling_example.d \
./src/xuartlite_intr_example.d \
./src/xuartlite_polled_example.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MicroBlaze gcc compiler'
	mb-gcc -Wall -O0 -g3 -c -fmessage-length=0 -MT"$@" -I../../dmauart_bsp/microblaze_1/include -mlittle-endian -mcpu=v11.0 -mxl-soft-mul -Wl,--no-relax -ffunction-sections -fdata-sections -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


