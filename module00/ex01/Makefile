MCU = atmega328p
F_CPU = 16000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
DUDE = avrdude

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU)

all: hex flash

hex: main.c
	$(CC) $(CFLAGS) -o main.elf main.c
	$(OBJCOPY) -O ihex main.elf main.hex

flash: main.hex
	$(DUDE) -c arduino -p $(MCU) -P /dev/ttyUSB0 -b 115200 -U flash:w:main.hex

clean:
	rm -f *.elf *.hex