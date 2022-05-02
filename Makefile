# Copyright (c) 2022 Renze Nicolai
# Copyright (c) 2022 Jana Marie Hemsing
# SPDX-License-Identifier: MIT

INSTALL_PREFIX := $PWD
BUILD_DIR := build
GEN_DIR := generated

BL_BIN := mch2022_bootloader.bin
FW_BIN := mch2022_firmware.bin
CB_UF2 := mch2022.uf2

all: firmware combine flash
	@echo "All tasks completed"

firmware: $(BUILD_DIR) $(GEN_DIR)
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all
	python genuf2.py $(BUILD_DIR)/$(BL_BIN) $(BUILD_DIR)/$(FW_BIN) $(BUILD_DIR)/$(CB_UF2)
	
flash:
	picotool load $(BUILD_DIR)/$(CB_UF2)
	picotool reboot

clean:
	rm -r $(BUILD_DIR) $(GEN_DIR)

install_rules:
	cp tools/99-pico.rules /etc/udev/rules.d/
	@echo "reload rules with:"
	@echo "\tudevadm control --reload-rules"
	@echo "\tudevadm trigger"

$(BUILD_DIR):
	mkdir -p $@

$(GEN_DIR):
	mkdir -p $@
