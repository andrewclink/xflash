TARGET = xflash
LIBS = -lm
CCPATH =
CC = gcc
CFLAGS = -g -Wall -std=gnu99

ifeq ($(CROSS),1)
	PATH_DIR=/home/andrew/backfire/staging_dir/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.30.1
	BASE_DIR=/home/andrew/backfire/staging_dir/target-mipsel_uClibc-0.9.30.1/usr/
	CCPATH=$(PATH_DIR)/bin

	CFLAGS += -I$(PATH_DIR)/include
	CFLAGS += -I$(BASE_DIR)/include
	CFLAGS += -I$(BASE_DIR)/include/libusb-1.0
	LIBS   += -L$(BASE_DIR)/lib
	LIBS   += -lusb-1.0
	CC := $(CCPATH)/mipsel-openwrt-linux-uclibc-gcc 
	LD := $(CCPATH)/mipsel-openwrt-linux-uclibc-ld
else
	CFLAGS += $(shell pkg-config --cflags libusb-1.0)
	LIBS +=  $(shell pkg-config --libs libusb-1.0)
endif

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
ifeq ($(CROSS),1)
	@echo "Doing Cross Compile"
endif
	$(CC) $(CFLAGS) -c $< -o $@


.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)