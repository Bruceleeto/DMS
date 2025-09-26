TARGET = raylib.elf
OBJS = main.o dms.o gl_png.o romdisk.o
KOS_ROMDISK_DIR = romdisk
CONVERTER_DIR = ../converter
STRIPPY = $(CONVERTER_DIR)/strippy

CFLAGS = -g \
         -fomit-frame-pointer -flto -fbuiltin -ffast-math -ffp-contract=fast -mfsrra -mfsca \
         -fmerge-all-constants  \
         -ftree-vectorize -I${KOS_PORTS}/include/raylib

all: rm-elf convert-textures convert-models $(TARGET)

convert-textures:
	@mkdir -p $(KOS_ROMDISK_DIR)
	@find assets -name "*.png" | while read png_file; do \
		rel_path=$${png_file#assets/}; \
		dir_path=$$(dirname "$$rel_path"); \
		base_name=$$(basename $$png_file .png); \
		mkdir -p "$(KOS_ROMDISK_DIR)/$$dir_path"; \
		echo "Converting $$png_file to $(KOS_ROMDISK_DIR)/$$dir_path/$$base_name.tex"; \
		$(KOS_BASE)/utils/pvrtex/pvrtex -i $$png_file -o "$(KOS_ROMDISK_DIR)/$$dir_path/$$base_name.tex" -f auto -c; \
	done

$(STRIPPY):
	@echo "Building strippy converter..."
	$(MAKE) -C $(CONVERTER_DIR)

convert-models: $(STRIPPY)
	@find assets -name "*.glb" | while read glb_file; do \
		rel_path=$${glb_file#assets/}; \
		dir_path=$$(dirname "$$rel_path"); \
		base_name=$$(basename $$glb_file .glb); \
		mkdir -p "$(KOS_ROMDISK_DIR)/$$dir_path"; \
		echo "Converting $$glb_file to $(KOS_ROMDISK_DIR)/$$dir_path/$$base_name.dms"; \
		cp $$glb_file .; \
		$(STRIPPY) $$(basename $$glb_file); \
		mv $$(basename $$glb_file .glb).dms "$(KOS_ROMDISK_DIR)/$$dir_path/"; \
		rm $$(basename $$glb_file); \
	done
	@# Copy any other non-PNG/GLB files preserving directory structure
	@find assets -type f -not -name "*.png" -not -name "*.glb" | while read file; do \
		rel_path=$${file#assets/}; \
		dir_path=$$(dirname "$$rel_path"); \
		mkdir -p "$(KOS_ROMDISK_DIR)/$$dir_path"; \
		cp "$$file" "$(KOS_ROMDISK_DIR)/$$rel_path"; \
	done

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	-rm -rf $(KOS_ROMDISK_DIR)/*

rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc $(CFLAGS) -o $(TARGET) $(OBJS) -lraylib  -lGL -lm -lkosutils 

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)