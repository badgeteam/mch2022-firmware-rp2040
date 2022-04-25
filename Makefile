# Copyright (c) 2022 Jana Marie Hemsing
# SPDX-License-Identifier: MIT

INSTALL_PREFIX := $PWD
BUILD_DIR := build
GEN_DIR := generated

UF2 := mch2022_firmware.uf2

all: firmware flash
	@echo "All tasks completed"

firmware: $(BUILD_DIR) $(GEN_DIR)
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all install

flash:
	picotool load $(BUILD_DIR)/$(UF2)
	picotool reboot

clean:
	rm -rf $(BUILD_DIR) $(GEN_DIR)

install_rules:
	cp tools/99-pico.rules /etc/udev/rules.d/
	@echo "reload rules with:"
	@echo "\tudevadm control --reload-rules"
	@echo "\tudevadm trigger"

$(BUILD_DIR):
	mkdir -p $@

$(GEN_DIR):
	mkdir -p $@
