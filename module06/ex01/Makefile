MCU = atmega328p
F_CPU = 16000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
DUDE = avrdude
DEVISE = /dev/tty.usbserial-10
UART_BAUDRATE = 115200

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os

all: hex flash

hex: main.c aht20.c uart.c i2c.c
	$(CC) $(CFLAGS) -o main.elf main.c aht20.c uart.c i2c.c
	$(OBJCOPY) -O ihex main.elf main.hex

flash: main.hex
	$(DUDE) -c arduino -p $(MCU) -P $(DEVISE) -b $(UART_BAUDRATE) -U flash:w:main.hex

screen: 
	screen $(DEVISE) $(UART_BAUDRATE)

clean:
	rm -f main.elf main.hex