# shitty makefile, send help D:
# Jana Marie Hemsing 2022

INSTALL_PREFIX := $PWD
BUILD_DIR := build
GEN_DIR := generated

UF2 := mch2022_firmware.uf2

all: firmware flash
	@echo "all done :3"

firmware: $(BUILD_DIR) $(GEN_DIR) env_vars
	cd $(BUILD_DIR); cmake -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX ..
	$(MAKE) -C $(BUILD_DIR) --no-print-directory all install

flash:
	picotool load $(BUILD_DIR)/$(UF2)
	picotool reboot

clean:
	rm -r $(BUILD_DIR) $(GEN_DIR)

install_rules:
	cp tools/99-pico.rules /etc/udev/rules.d/
	@echo "reload rules with:"
	@echo "\tudevadm control --reload-rules"
	@echo "\tudevadm trigger"

env_vars:
	set -e -u # this does not work yet, please run manually, this will be run in a child process, but not the parrent console

$(BUILD_DIR):
	mkdir -p $@

$(GEN_DIR):
	mkdir -p $@
