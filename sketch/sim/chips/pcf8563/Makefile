# SPDX-License-Identifier: MIT
# Build the PCF8563 Wokwi custom chip (layout of the official Wokwi chip template,
# github.com/wokwi/inverter-chip). Needs wasi-sdk: WASI_SDK_PATH=/path/to/wasi-sdk
#   make        -> dist/chip.wasm + dist/chip.json
#   make zip    -> dist/chip.zip (release payload, as the template's release workflow)
#   make test   -> host unit test (native cc, stubbed Wokwi API)

WASI_SDK_PATH ?= /opt/wasi-sdk
CC      = $(WASI_SDK_PATH)/bin/clang
HOST_CC ?= cc
SOURCES = pcf8563.chip.c
TARGET  = dist/chip.wasm
CFLAGS  = --target=wasm32-wasip1 --sysroot $(WASI_SDK_PATH)/share/wasi-sysroot \
          -O2 -Wall -Wextra -Wno-unused-function -Werror -nostartfiles \
          -Wl,--import-memory -Wl,--export-table -Wl,--no-entry

.PHONY: all clean zip test
all: $(TARGET) dist/chip.json

dist:
	mkdir -p dist

$(TARGET): $(SOURCES) wokwi-api.h | dist
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

dist/chip.json: pcf8563.chip.json | dist
	cp $< $@

zip: all
	cd dist && rm -f chip.zip && zip -9 -q chip.zip chip.json chip.wasm

test: | dist
	@# why: wokwi rejects unknown keys at simulation start (chip.schema.json, additionalProperties: false)
	python3 -c 'import json,sys; k=set(json.load(open("pcf8563.chip.json"))); bad=k-{"name","author","docs","pins","controls","display"}; sys.exit("chip.json: unknown keys %s" % bad if bad else 0)'
	$(HOST_CC) -std=c11 -g -Wall -Wno-unknown-attributes -Wno-attributes -Wno-unused-function -o dist/host_test test/host_test.c
	./dist/host_test

clean:
	rm -rf dist
