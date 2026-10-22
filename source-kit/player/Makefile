NAME := snatcher_nds_resources_r22
GAME_TITLE := SNATCHER
GAME_SUBTITLE := Korean V0.95
GAME_ICON := assets/launcher-icon.gif
LIBS := -lmm9 -lnds9 -lm
LIBDIRS := $(BLOCKSDS)/libs/maxmod
NITROFSDIR := build/runtime-nitrofs-r22
INCLUDEDIRS := include
ARM7ELF := build/arm7.elf
GAME_AUTHOR := Mega CD to Nintendo DS
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
PYTHON ?= $(if $(SNATCHER_PYTHON),$(SNATCHER_PYTHON),python3)
# Leave6KiB for the user stack when placing renderer tables in fast memory.
# The SDK linker rejects DTCM data that would cross this reservation.
LDFLAGS += -Wl,--defsym=__usr_stack_size=0x1800
LDFLAGS += -Wl,--undefined=reception_observe_layout

# Profiling is a compile-time option. Make does not otherwise notice a changed
# command line, which could leave timing instrumentation in a release build.
PROFILE_MODE_FILE := $(BUILDDIR)/profile-mode.json
.PHONY: profile-mode-force
$(PROFILE_MODE_FILE): profile-mode-force
	@"$(PYTHON)" -B tools/configure_profile_build_r22.py "$@" "$(if $(OPENING_PROFILE),1,0)" "$(if $(FLIGHT_PROFILE),1,0)"
$(addprefix $(BUILDDIR)/source/,$(addsuffix .c.o,main opening_player native_opening resource_background resource_opening resource_music native_entry music_player reception_background)): $(PROFILE_MODE_FILE)

# The native plane compositor is updated every source VBlank. ARM code avoids
# Thumb register spills in its inner loops; PCM uses the same fast path.
$(BUILDDIR)/source/resource_background.c.o: CFLAGS += -marm -O3
ifneq ($(OPENING_PROFILE),)
$(BUILDDIR)/source/main.c.o $(BUILDDIR)/source/opening_player.c.o: CFLAGS += -DSNATCHER_OPENING_PROFILE
$(BUILDDIR)/source/native_opening.c.o: CFLAGS += -DSNATCHER_OPENING_PROFILE
$(BUILDDIR)/source/resource_background.c.o: CFLAGS += -DSNATCHER_OPENING_PROFILE
$(BUILDDIR)/source/resource_opening.c.o: CFLAGS += -DSNATCHER_OPENING_PROFILE
endif

$(BUILDDIR)/source/native_entry.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_frame.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_graphics.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_pcm_output.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_pcm_chip.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_shared_sound.c.o $(BUILDDIR)/source/resource_pcm_driver.c.o $(BUILDDIR)/source/music_player.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/resource_music.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/video_filters.c.o: CFLAGS += -marm -O3
$(BUILDDIR)/source/native_opening.c.o: CFLAGS += -marm -O3
ifneq ($(FLIGHT_PROFILE),)
$(BUILDDIR)/source/music_player.c.o: CFLAGS += -DSNATCHER_AUDIO_HASH
$(BUILDDIR)/source/main.c.o $(BUILDDIR)/source/reception_background.c.o: CFLAGS += -DSNATCHER_ENTRY_PROFILE
$(BUILDDIR)/source/resource_music.c.o: CFLAGS += -DSNATCHER_ENTRY_PROFILE
$(BUILDDIR)/source/native_entry.c.o $(BUILDDIR)/source/music_player.c.o: CFLAGS += -DSNATCHER_ENTRY_PROFILE -DSNATCHER_OPENING_PROFILE
$(BUILDDIR)/source/resource_background.c.o: CFLAGS += -DSNATCHER_OPENING_PROFILE
endif

$(BUILDDIR)/source/ym2612.c.o $(BUILDDIR)/source/resource_fm.c.o: CFLAGS += -marm -O3 -D_ASM_YM2612_C
LDFLAGS += -Wl,--wrap=malloc

.PHONY: $(NITROFSDIR)
$(NITROFSDIR):
	"$(PYTHON)" -B tools/check_native_fm_r22.py
	"$(PYTHON)" -B tools/stage_runtime_resources_r22.py

# Storage wrappers depend on the audited BlocksDS FatFs ABI.
LDFLAGS += -Wl,--wrap=nitroFSInit -Wl,--wrap=fatInitLookupCache
.PHONY: storage-sdk-check
storage-sdk-check:
	"$(PYTHON)" -B include/storage_compat/check_sdk.py "$(BLOCKSDS)/libs/libnds/lib/libnds9.a"
$(ELF) $(BUILDDIR)/source/resource_storage.c.o: | storage-sdk-check

CFLAGS += -Wframe-larger-than=512 -fstack-usage

$(BUILDDIR)/source/adapter.c.o $(BUILDDIR)/source/m68kcpu.c.o $(BUILDDIR)/source/forecourt_fm.c.o: CFLAGS += -marm -O3 -DLSB_FIRST

$(BUILDDIR)/source/resource_live_portrait.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/resource_entry.c.o $(BUILDDIR)/source/resource_tasks.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/resource_portrait_history.c.o $(BUILDDIR)/source/portrait_live.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/resource_opening.c.o $(BUILDDIR)/source/resource_opening_presentation.c.o $(BUILDDIR)/source/resource_scene.c.o $(BUILDDIR)/source/resource_actor.c.o $(BUILDDIR)/source/resource_palette.c.o $(BUILDDIR)/source/resource_scroll.c.o $(BUILDDIR)/source/resource_cards.c.o $(BUILDDIR)/source/resource_main_scroll.c.o $(BUILDDIR)/source/resource_sprites.c.o $(BUILDDIR)/source/resource_callback.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/pcm_output_mono.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/resource_picture_row.c.o $(BUILDDIR)/source/native_entry_display.c.o $(BUILDDIR)/source/resource_vdp_history.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/ui_theme.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/resource_live_portrait.c.o $(BUILDDIR)/source/resource_portrait_history.c.o: CFLAGS += -marm -O3

$(BUILDDIR)/source/practice_expand.c.o: CFLAGS += -marm -O3
