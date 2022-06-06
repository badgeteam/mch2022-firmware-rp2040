# Copyright (c) 2022 Nicolai Electronics
# Copyright (c) 2022 Jana Marie Hemsing
# SPDX-License-Identifier: MIT

INSTALL_PREFIX := $PWD
BUILD_DIR := build

BL_BIN := mch2022_bootloader.bin
FW_BIN := mch2022_firmware.bin
CB_UF2 := mch2022.uf2

.PHONY: all firmware flash clean install_rules $(BUILD_DIR)

all: build flash
	@echo "All tasks completed"

build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DCMAKE_BUILD_TYPE=Release ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all
	python genuf2.py $(BUILD_DIR)/$(BL_BIN) $(BUILD_DIR)/$(FW_BIN) $(BUILD_DIR)/$(CB_UF2)

debug:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX -DCMAKE_BUILD_TYPE=Debug ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all

flash:
	picotool load $(BUILD_DIR)/$(CB_UF2)
	picotool reboot

clean:
	rm -rf $(BUILD_DIR)

install_rules:
	cp tools/99-pico.rules /etc/udev/rules.d/
	@echo "reload rules with:"
	@echo "\tudevadm control --reload-rules"
	@echo "\tudevadm trigger"
