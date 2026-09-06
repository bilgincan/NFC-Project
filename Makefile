# Thin wrapper around the CMake build so you can just type `make build` /
# `make flash` instead of the full cmake invocations.

BUILD_DIR   := build
TARGET      := nfc_door_opener
BIN         := $(BUILD_DIR)/$(TARGET).bin
FLASH_ADDR  := 0x8000000

.PHONY: build flash clean

build:
	rm -rf $(BUILD_DIR)
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j

flash: build
	st-flash --reset write $(BIN) $(FLASH_ADDR)

clean:
	rm -rf $(BUILD_DIR)
