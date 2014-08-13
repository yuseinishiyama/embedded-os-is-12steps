PREFIX	= $(HOME)/Workspace/Lessons/embedded-os-12steps
ARCH 	= h8300-elf
BINDIR 	= $(PREFIX)/tools/bin
ADDNAME = $(ARCH)-
SERIAL = /dev/tty.usbserial-FTXPZO9L

AR	= $(BINDIR)/$(ADDNAME)ar
AS	= $(BINDIR)/$(ADDNAME)as
CC	= $(BINDIR)/$(ADDNAME)gcc
LD	= $(BINDIR)/$(ADDNAME)ld
NM	= $(BINDIR)/$(ADDNAME)nm
OBJCOPY = $(BINDIR)/$(ADDNAME)objcopy
OBJDUMP = $(BINDIR)/$(ADDNAME)objdump
RANLIB  = $(BINDIR)/$(ADDNAME)ranlib
STRIP   = $(BINDIR)/$(ADDNAME)strip

OBJS = startup.o main.o interrupt.o
OBJS += lib.o serial.o

#source of kozos
OBJS += kozos.o syscall.o test09_1.o test09_2.o test09_3.o

TARGET = kozos

CFLAGS = -Wall -mh -nostdinc -nostdlib -fno-builtin
CFLAGS += -I.
CFLAGS += -Os
CFLAGS += -DKOZOS

LFLAGS = -static -T ld.scr -L.

.SUFFIXES: .c .o
.SUFFIXES: .s .o
.SUFFIXES: .S .o

all :		$(TARGET)
$(TARGET) :	$(OBJS)
		$(CC) $(OBJS) -o $(TARGET) $(CFLAGS) $(LFLAGS)
		cp $(TARGET) $(TARGET).elf
		$(STRIP) $(TARGET)

TAGS :		$(TARGET)
		etags *.[ch]

.c.o :		$<
		$(CC) -c $(CFLAGS) $<

.s.o :		$<
		$(CC) -c $(CFLAGS) $<

.S.o :		$<
		$(CC) -c $(CFLAGS) $<

load :		$(TARGET)
		kz_xmodem kozos $(SERIAL)

run :
		sudo cu -l $(SERIAL)

clean :
		rm -f $(OBJS) $(TARGET) $(TARGET).elf
