# shitty makefile, send help D:
# Jana Marie Hemsing 2022

INSTALL_PREFIX := $(shell pwd)
BUILD_DIR := build
GEN_DIR := generated

UF2 := mch2022_firmware.uf2

all: firmware flash
	@echo "all done :3"

firmware: $(BUILD_DIR) $(GEN_DIR)
	echo $(INSTALL_PREFIX)
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all

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
