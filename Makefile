# SPDX-License-Identifier: GPL-3.0-or-later
PROJECT := evistdrive_hmi
VERSION := 10.24
BUILD := build
DIST := dist
ARM_CC ?= clang
ARM_OBJCOPY ?= llvm-objcopy
HOST_CC ?= clang
PYTHON ?= python3

# Source identity embedded in every image: the abbreviated hash of the last
# commit + -DIRTY when the working tree differs from HEAD (UNKNOWN-DIRTY outside
# a git checkout, e.g. an extracted archive). No timestamps, so the same
# sources always yield byte-identical binaries.
GIT_COMMIT := $(shell git log -1 --format=%h --abbrev=10 -- . 2>/dev/null | grep . || echo UNKNOWN)
GIT_DIRTY := $(shell git diff --quiet HEAD -- . 2>/dev/null || echo -DIRTY)
GIT_ID := $(GIT_COMMIT)$(GIT_DIRTY)

INCLUDES := -Iinclude -Iplatform/dpc245
FW_SOURCES := $(sort $(wildcard src/*.c) $(wildcard platform/dpc245/*.c))
HOST_MODULES := \
 src/advanced_settings.c src/assist_profile.c src/bafang_can.c src/dpc245_clock_tree.c src/bafang_config.c src/bafang_service.c \
 src/can_router.c src/controller_write.c src/dpc245_app.c src/dpc245_backlight_model.c src/dpc245_ble_proto.c src/dpc245_hmi_service.c \
 src/dpc245_keys.c src/dpc245_key_sampler.c src/dpc245_persist.c src/dpc245_rtc.c src/dpc245_hmi_control.c src/dpc245_runtime.c src/dpc245_settings.c src/dpc245_ble_app.c \
 src/dpc245_tx.c src/dpc245_update.c src/dpc245_v3_profile.c src/evistdrive_adapter.c src/evist_preferences.c \
 src/light_controller.c src/menu_model.c src/dpc245_recovery_trigger.c src/boot_splash.c src/ride_mode.c src/native_store.c src/ui_strip.c src/loop_stats.c
HOST_SOURCES := tests/test_core.c $(HOST_MODULES)
HOST_SANITIZE := -fsanitize=address,undefined -fno-sanitize-recover=all

ARM_COMMON := --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -std=c11 -ffreestanding \
 -fno-builtin -fdata-sections -ffunction-sections -Wall -Wextra -Werror \
 -Wframe-larger-than=512 -ffile-prefix-map=$(CURDIR)=. $(INCLUDES) \
 -DEVIST_BUILD_GIT=\"$(GIT_ID)\" $(EXTRA_DEFS)
# Optional per-build feature overrides, e.g. EXTRA_DEFS=-DEVIST_ENABLE_PERSISTENCE=0 (BRINGUP: no SPI writes).
EXTRA_DEFS ?=
LINK_COMMON := -nostdlib -Wl,--gc-sections -Wl,-T,platform/dpc245/dpc245.ld

PROFILE_product := 0
PROFILE_recovery_fault := 1
PROFILE_probe := 2

.PHONY: all firmware debug recovery-fault probe images test validate check clean package update info sim screenshots update-full
all: firmware

$(BUILD):
	mkdir -p $(BUILD)

# $(call arm_image,<output stem>,<opt level>,<profile name>)
define arm_image
	$(ARM_CC) $(ARM_COMMON) $(2) -g3 -DEVIST_BUILD_PROFILE=$(PROFILE_$(3)) $(FW_SOURCES) $(LINK_COMMON) \
	  -Wl,-Map,$(BUILD)/$(1).map -o $(BUILD)/$(1).elf
	$(ARM_OBJCOPY) -O binary $(BUILD)/$(1).elf $(BUILD)/$(1).bin
	$(PYTHON) tools/check_firmware.py $(BUILD)/$(1).bin $(BUILD)/$(1).elf
endef

firmware: $(BUILD)
	$(call arm_image,$(PROJECT),-Os,product)

debug: $(BUILD)
	$(call arm_image,$(PROJECT)_debug,-Og,product)

recovery-fault: $(BUILD)
	$(call arm_image,$(PROJECT)_recovery_fault,-Os,recovery_fault)

probe: $(BUILD)
	$(call arm_image,$(PROJECT)_probe,-Os,probe)

images: firmware recovery-fault probe

test: $(BUILD)
	$(PYTHON) tests/check_persistence_service.py
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror $(INCLUDES) $(HOST_SOURCES) -o $(BUILD)/test_core
	$(BUILD)/test_core
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(INCLUDES) tests/test_menu_tree.c $(HOST_MODULES) -o $(BUILD)/test_menu_tree
	$(BUILD)/test_menu_tree
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(INCLUDES) tests/test_app_settings.c $(HOST_MODULES) -o $(BUILD)/test_app_settings
	$(BUILD)/test_app_settings
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror -DEVIST_ENABLE_CONFIG_WRITES=0 $(INCLUDES) tests/test_app_settings.c $(HOST_MODULES) -o $(BUILD)/test_app_settings_off
	$(BUILD)/test_app_settings_off
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(HOST_SANITIZE) $(INCLUDES) tests/test_config.c $(HOST_MODULES) -o $(BUILD)/test_config
	$(BUILD)/test_config
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(HOST_SANITIZE) $(INCLUDES) tests/test_persistence_async.c $(HOST_MODULES) -o $(BUILD)/test_persistence_async
	$(BUILD)/test_persistence_async
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(HOST_SANITIZE) $(INCLUDES) tests/test_native_store.c platform/dpc245/dpc245_spi_mem.c $(HOST_MODULES) -o $(BUILD)/test_native_store
	$(BUILD)/test_native_store
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(HOST_SANITIZE) $(INCLUDES) tests/test_ui_strip.c sim/host_lcd.c src/ui_dashboard.c platform/dpc245/minifont.c platform/dpc245/ui_font1.c $(HOST_MODULES) -o $(BUILD)/test_ui_strip
	$(BUILD)/test_ui_strip
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(HOST_SANITIZE) -DDPC245_BUTTONS_HOST_TEST $(INCLUDES) tests/test_theme_key_session.c platform/dpc245/dpc245_buttons.c $(HOST_MODULES) -o $(BUILD)/test_theme_key_session
	$(BUILD)/test_theme_key_session
	mkdir -p $(BUILD)/sim
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror -DUI_FONT1_TRACE $(INCLUDES) sim/host_lcd.c sim/render_screens.c src/ui_dashboard.c platform/dpc245/minifont.c platform/dpc245/ui_font1.c $(HOST_MODULES) -o $(BUILD)/render_screens
	$(BUILD)/render_screens $(BUILD)/sim > $(BUILD)/sim/layout.txt || (cat $(BUILD)/sim/layout.txt; exit 1)
	@grep "layout issues" $(BUILD)/sim/layout.txt

validate:
	$(PYTHON) tools/check_repository.py
	$(PYTHON) tools/check_requirements.py
	$(PYTHON) tools/generate_function_reference.py --check
	$(PYTHON) tools/build_update_image.py --self-test

check: clean validate test firmware debug recovery-fault probe update update-full
	@echo "EVistDrive HMI: all checks passed"

clean:
	rm -rf $(BUILD) $(DIST)

# Host screen preview: renders the real dashboard/menu code into 320x170 frames,
# then writes sim/out/index.html with pixel coordinates (no hardware, no flashing).
#   make sim
sim: $(BUILD)
	mkdir -p sim/out
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror -DUI_FONT1_TRACE $(INCLUDES) sim/host_lcd.c sim/render_screens.c src/ui_dashboard.c platform/dpc245/minifont.c platform/dpc245/ui_font1.c $(HOST_MODULES) -o $(BUILD)/render_screens
	$(BUILD)/render_screens sim/out
	$(PYTHON) tools/render_preview.py sim/out

# README gallery: nine fixed UI states, rendered without a coordinate grid.
# Intermediate RGB565 frames and PNGs stay in sim/out/screenshots/.
SCREENSHOT_NAMES := standard_dashboard yellow_dashboard arc_dashboard wedge_dashboard overheat_warning main_menu themes_page temp_view_limits advanced_assist
screenshots: $(BUILD)
	mkdir -p sim/out/screenshots docs/images
	$(HOST_CC) -std=c11 -O1 -g -Wall -Wextra -Werror $(INCLUDES) sim/host_lcd.c sim/render_screenshots.c src/ui_dashboard.c platform/dpc245/minifont.c platform/dpc245/ui_font1.c $(HOST_MODULES) -o $(BUILD)/render_screenshots
	$(BUILD)/render_screenshots sim/out/screenshots
	$(PYTHON) tools/render_preview.py --no-grid sim/out/screenshots
	@for n in $(SCREENSHOT_NAMES); do cp sim/out/screenshots/$$n.png docs/images/$$n.png || exit 1; done

info:
	@echo "source   $(GIT_ID)"
	@$(ARM_CC) --version | head -1
	@$(ARM_OBJCOPY) --version | head -1

# Installation files are built from the application image alone. make update
# builds the short layout (header + application), make update-full the full
# layout with a blank fill (docs/BUILD_AND_RELEASE.md). No other input is used.
NO_TEMPLATE := @test -z "$(UPDATE_TEMPLATE)" || (echo "UPDATE_TEMPLATE is not supported: run make update without arguments"; exit 2)

package: check
	$(NO_TEMPLATE)
	mkdir -p $(DIST)
	for p in $(PROJECT) $(PROJECT)_recovery_fault $(PROJECT)_probe; do \
	  cp $(BUILD)/$$p.elf $(BUILD)/$$p.map $(BUILD)/$$p.bin $(DIST)/ || exit 1; \
	  $(PYTHON) tools/build_update_image.py --short \
	    --app $(BUILD)/$$p.bin --out $(DIST)/$${p}_$(VERSION)_$(GIT_ID)_update.bin || exit 1; \
	done
	cp docs/TECHNICAL_REQUIREMENTS.md $(DIST)/TECHNICAL_REQUIREMENTS.md
	$(PYTHON) tools/write_release_manifest.py $(DIST) --source "$(GIT_ID)" \
	  --toolchain "$$($(ARM_CC) --version | head -1)"

update: firmware
	$(NO_TEMPLATE)
	$(PYTHON) tools/build_update_image.py --short --app $(BUILD)/$(PROJECT).bin --out $(BUILD)/EVistDrive_HMI_$(VERSION)_Update.bin
	$(PYTHON) tools/build_update_image.py --inspect $(BUILD)/EVistDrive_HMI_$(VERSION)_Update.bin

# Full layout: header, application and blank fill (docs/BUILD_AND_RELEASE.md).
update-full: firmware
	$(NO_TEMPLATE)
	$(PYTHON) tools/build_update_image.py --app $(BUILD)/$(PROJECT).bin --out $(BUILD)/EVistDrive_HMI_$(VERSION)_Update_full.bin
	$(PYTHON) tools/build_update_image.py --inspect $(BUILD)/EVistDrive_HMI_$(VERSION)_Update_full.bin
