TARGET = xflash
LIBS = -lm $(shell pkg-config --libs libusb-1.0)
CCPATH =
CC = gcc
CFLAGS = -g -O0 -Wall 

.PHONY: default all clean cross cross-env

default: $(TARGET)
all: default
cross: $(TARGET)-cross
cross-env:
	@echo "Setting up cross environment"
	CCPATH=/backfire/staging_dir/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.30.1/bin
	CC=$(CCPATH)/mipsel-openwrt-linux-uclibc-gcc 
	LD=$(CCPATH)/mipsel-openwrt-linux-uclibc-ld
	

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@
	
$(TARGET)-cross: cross-env $(OBJECTS)
	@echo "Doing Cross Compile"
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@
	

clean:
	-rm -f *.o
	-rm -f $(TARGET)