TARGET = cube.elf
OBJS = main.o dms.o gl_png.o romdisk.o
KOS_ROMDISK_DIR = romdisk
CONVERTER_DIR = ../converter
STRIPPY = $(CONVERTER_DIR)/strippy
TOY_DIR = ../ToyTest

CFLAGS = -O3 \
         -fomit-frame-pointer -flto -fbuiltin -ffast-math -ffp-contract=fast -mfsrra -mfsca \
         -fmerge-all-constants  \
         -ftree-vectorize \
         -I$(TOY_DIR)/source

all: rm-elf convert-textures convert-models build-toy $(TARGET)

build-toy:
	@if [ ! -f $(TOY_DIR)/libtoy.a ]; then \
		echo "Building Toy library..."; \
		$(MAKE) -C $(TOY_DIR); \
	fi

convert-textures:
	mkdir -p $(KOS_ROMDISK_DIR)
	@for png_file in assets/*.png; do \
		base_name=$$(basename $$png_file .png); \
		echo "Converting $$png_file to $(KOS_ROMDISK_DIR)/$$base_name.tex"; \
		$(KOS_BASE)/utils/pvrtex/pvrtex -i $$png_file -o $(KOS_ROMDISK_DIR)/$$base_name.tex -f RGB565 -c -m; \
	done

$(STRIPPY):
	@echo "Building strippy converter..."
	$(MAKE) -C $(CONVERTER_DIR)

convert-models: $(STRIPPY)
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
	@# Copy any other non-PNG/GLB files from assets
	@find assets -type f -not -name "*.png" -not -name "*.glb" -exec cp {} $(KOS_ROMDISK_DIR)/ \;

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -f $(KOS_ROMDISK_DIR)/*.tex
	-rm -f $(KOS_ROMDISK_DIR)/*.dms

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc $(CFLAGS) -o $(TARGET) $(OBJS) -L$(TOY_DIR) -ltoy -lGL -lm -lkosutils 

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)