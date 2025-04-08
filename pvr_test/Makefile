TARGET = pvrtest.elf
OBJS = pvrtest.o dms.o  romdisk.o
KOS_ROMDISK_DIR = romdisk

# Aggressive Dreamcast-specific optimizations
CFLAGS += -O3 \
          -fomit-frame-pointer -flto -fbuiltin -ffast-math -ffp-contract=fast -mfsrra -mfsca \
          -fmerge-all-constants -funroll-loops -fno-PIC -fipa-pta  \
          -ftree-vectorize -I$(KOS_BASE)/utils


all: rm-elf $(TARGET)
include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)

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