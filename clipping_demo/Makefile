TARGET = test.elf

OBJS =   main.o perf_profile.o camera.o
OBJS +=  dms.o matrix.o prim_buffer.o  romdisk.o

KOS_ROMDISK_DIR = romdisk

CFLAGS += -O3 \
          -fomit-frame-pointer -flto -fbuiltin -ffast-math  -ffp-contract=fast -mfsrra -mfsca \
          -fmerge-all-constants -funroll-loops -fno-PIC -fipa-pta \
          -ftree-vectorize -I$(KOS_BASE)/utils

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean: rm-elf
	-rm -f $(OBJS)
	
rm-elf:
	-rm -f $(TARGET) romdisk.*

$(TARGET): $(OBJS)
	kos-cc -o $(TARGET) $(OBJS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	-rm -f $(OBJS) romdisk.img
	$(KOS_STRIP) $(TARGET)
