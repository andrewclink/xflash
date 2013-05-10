TARGET = xflash
LIBS = -lm
CCPATH =
CC = gcc
CFLAGS = -g -Wall -std=gnu99

INSTALL_DIR = /Users/andrew/bin

ifeq ($(CROSS),1)
	STAGING_DIR=/home/andrew/backfire/staging_dir
	PATH_DIR=$(STAGING_DIR)/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.30.1
	BASE_DIR=$(STAGING_DIR)/target-mipsel_uClibc-0.9.30.1/usr
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
install: all
ifeq ($(CROSS),1)
	@echo "Doing remote install"
	scp $(TARGET) root@172.16.1.40:/bin
else
	cp $(TARGET) $(INSTALL_DIR)
endif

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
ifeq ($(CROSS),1)
	@echo "Doing Cross Compile"
	STAGING_DIR=$(STAGING_DIR) $(CC) $(CFLAGS) -c $< -o $@
else
	$(CC) $(CFLAGS) -c $< -o $@
endif


.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
ifeq ($(CROSS),1)
	@echo "Finishing Cross Compile"
	STAGING_DIR=$(STAGING_DIR) $(CC) $(OBJECTS) -Wall $(LIBS) -o $@
else
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@
endif

clean:
	-rm -f *.o
	-rm -f $(TARGET)