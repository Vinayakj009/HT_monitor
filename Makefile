#!/bin/bash
ARCH = $(shell uname)
ifeq ($(ARCH), Linux)
    PACKAGE_BASE ?= $(HOME)/.arduino15/packages
    ARDUINO_BASE ?= /usr/share/arduino
    SERIAL_PORT ?= /dev/ttyUSB0
else ifeq ($(ARCH), Darwin)
    PACKAGE_BASE ?= $(HOME)/Library/Arduino15/packages
    ARDUINO_BASE ?= /Applications/Arduino.app/Contents/Java
    SERIAL_PORT ?= /dev/cu.SLAB_USB*
endif

FULLY_QUALIFIED_BOARD_NUMBER=esp8266:esp8266:generic:xtal=80,vt=flash,exception=disabled,ssl=all,ResetMethod=ck,CrystalFreq=26,FlashFreq=40,FlashMode=dio,eesz=4M3M,led=2,sdk=nonosdk221,ip=lm2f,dbg=Disabled,lvl=None____,wipe=none,baud=115200
TARGET = $(notdir $(realpath .))
COMMON_COMPILATION_PROPERTIES = -logger=machine -hardware $(ARDUINO_BASE)/hardware \
                                -hardware $(PACKAGE_BASE) \
                                -hardware $(HOME)/Documents/Arduino/hardware \
                                -tools $(ARDUINO_BASE)/tools-builder \
                                -tools $(ARDUINO_BASE)/hardware/tools/avr \
                                -tools $(PACKAGE_BASE) \
                                -built-in-libraries $(ARDUINO_BASE)/libraries \
                                -libraries $(HOME)/Documents/Arduino/libraries \
                                -fqbn=$(FULLY_QUALIFIED_BOARD_NUMBER) \
                                -warnings=all \
                                -prefs=build.warn_data_percentage=75 \
                                -prefs=runtime.tools.mkspiffs.path=$(PACKAGE_BASE)/esp8266/tools/mkspiffs/2.5.0-3-20ed2b9 \
                                -prefs=runtime.tools.python.path=$(PACKAGE_BASE)/esp8266/tools/python/3.7.2-post1 \
                                -prefs=runtime.tools.xtensa-lx106-elf-gcc.path=$(PACKAGE_BASE)/esp8266/tools/xtensa-lx106-elf-gcc/2.5.0-3-20ed2b9 

CONF ?= All
TARGET_DEVICE_ID ?= 00248F1E
BIN_FILE ?= $(CURDIR)/$(CONF_TYPES)/temp/$(TARGET).ino.bin

ifeq ($(CONF),All)
    CONF_TYPES = PRODUCTION BETA
else ifeq ($(CONF),Debug)
    CONF_TYPES = BETA
else ifeq ($(CONF),Release)
    CONF_TYPES = PRODUCTION
else
    $(error Invalid build type '$(CONF)'. Available types are 'All', 'Debug' and 'Release')
endif

CLEAN_TYPES=$(addprefix clean_,$(CONF_TYPES))

.PHONY: all $(CONF_TYPES) clean upload

all: $(CONF_TYPES)


new_device: pull_id $(CONF_TYPES) upload

pull_id:
	@echo "Pulling device id"
	$(eval TARGET_DEVICE_ID := $(shell $(PACKAGE_BASE)/esp8266/tools/python/3.7.2-post1/python $(PACKAGE_BASE)/esp8266/hardware/esp8266/2.5.2/tools/esptool/esptool.py --chip esp8266 --port $(SERIAL_PORT) --baud 460800 chip_id | grep "Chip ID:" | cut -d ' ' -f 3))
	@echo "Device id is $(TARGET_DEVICE_ID)"

$(CONF_TYPES):
	mkdir -p $@/temp/ $@/cache/
	$(eval SPECIFIC_COMPILATION_PROPERTIES :=$(COMMON_COMPILATION_PROPERTIES) -build-path $(CURDIR)/$@/temp -build-cache $(CURDIR)/$@/cache -prefs=build.extra_flags=-DESP8266\ -DCLOUD_TYPE=CLOUD_TYPE_$@\ -DTARGET_DEVICE_ID=$(TARGET_DEVICE_ID) $(CURDIR)/$(TARGET).ino)
	$(ARDUINO_BASE)/arduino-builder -dump-prefs $(SPECIFIC_COMPILATION_PROPERTIES)
	$(ARDUINO_BASE)/arduino-builder -compile $(SPECIFIC_COMPILATION_PROPERTIES)

clean: $(CLEAN_TYPES)

clean_%:
	rm -rf $*/temp/* $*/cache/*

upload: upload_$(shell echo "$(CONF)" | tr '[:upper:]' '[:lower:]')

upload_all:
	@echo error: Invalid upload '$(CONF)'. Available types are 'Debug', 'Release'
	@exit

upload_%:
	$(PACKAGE_BASE)/esp8266/tools/python/3.7.2-post1/python $(PACKAGE_BASE)/esp8266/hardware/esp8266/2.5.2/tools/esptool/esptool.py --chip esp8266 --port $(SERIAL_PORT) --baud 460800 write_flash --flash_size=detect -fm dio 0 $(BIN_FILE)


