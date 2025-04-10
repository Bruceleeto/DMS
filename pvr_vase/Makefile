TARGET = pvr_vase.elf
OBJS = pvrtest.o dms.o romdisk.o
KOS_ROMDISK_DIR = romdisk
CONVERTER_DIR = ../converter
STRIPPY = $(CONVERTER_DIR)/strippy

CFLAGS += -O3 \
          -fomit-frame-pointer -flto -fbuiltin -ffast-math -ffp-contract=fast -mfsrra -mfsca \
          -fmerge-all-constants -funroll-loops -fno-PIC -fipa-pta \
          -ftree-vectorize -I$(KOS_BASE)/utils

all: rm-elf convert-textures convert-models $(TARGET)

convert-textures:
	mkdir -p $(KOS_ROMDISK_DIR)
	@for png_file in assets/*.png; do \
		if [ -f "$$png_file" ]; then \
			base_name=$$(basename $$png_file .png); \
			echo "Converting $$png_file to $(KOS_ROMDISK_DIR)/$$base_name.dt"; \
			$(KOS_BASE)/utils/pvrtex/pvrtex -i $$png_file -o $(KOS_ROMDISK_DIR)/$$base_name.dt -f RGB565 -c -m; \
		fi; \
	done

$(STRIPPY):
	@echo "Building strippy converter..."
	$(MAKE) -C $(CONVERTER_DIR)

convert-models: $(STRIPPY)
	mkdir -p $(KOS_ROMDISK_DIR)
	@for glb_file in assets/*.glb; do \
		if [ -f "$$glb_file" ]; then \
			base_name=$$(basename $$glb_file .glb); \
			echo "Converting $$glb_file to $(KOS_ROMDISK_DIR)/$$base_name.dms"; \
			cp $$glb_file .; \
			$(STRIPPY) $$(basename $$glb_file); \
			mv $$(basename $$glb_file .glb).dms $(KOS_ROMDISK_DIR)/; \
			rm $$(basename $$glb_file); \
		fi; \
	done
	@# Copy any other non-GLB/PNG files from assets if needed
	@find assets -type f -not -name "*.glb" -not -name "*.png" -exec cp {} $(KOS_ROMDISK_DIR)/ \;

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -f $(KOS_ROMDISK_DIR)/*.dms
	-rm -f $(KOS_ROMDISK_DIR)/*.dt

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS) -lkosutils -lm \
           -Wl,--gc-sections -Wl,--strip-all

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)