export PATH := $(HOME)/gcc-tms9900/bin:$(PATH)


ifneq ($(shell uname -s),Darwin)
  QUIET := status=none
endif


# Paths to TMS9900 compilation tools
GAS=tms9900-as
LD=tms9900-ld
CC=tms9900-gcc
CXX=tms9900-c++
OBJCOPY=tms9900-objcopy
OBJDUMP=tms9900-objdump

# Flags used during linking
# Refer to the linker rules in an external file
LDFLAGS:=\
  --script=linkfile

# Flags used during compiling
CFLAGS:=-std=gnu99 -O1 -g  -save-temps -Wall -Wextra -fomit-frame-pointer

# List of compiled objects used in executable
OBJECT_LIST:=\
  cart_header.o\
  main.o\
  crt0.o

# List of all files needed in executable
PREREQUISITES:=\
  $(OBJECT_LIST)\
  linkfile

# Recipe to compile the executable
all:
	$(MAKE) turmoilc.bin turmoil.lst turmoil.rpk turmoil.ea5
# Recursive make to get path to work on MacOS

main.o: graphics.h

graphics.h: turmoil.mag Makefile
	( echo "static const u8 number_ch[] = {" ;\
	gawk -F: -e '$$1=="CH" { if (i>=48 && i<58){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	echo "};" ;\
	echo "static const u8 rainbow_ch[] = {" ;\
	gawk -F: -e '$$1=="CH" { if (i>=96 && i<128){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	gawk -F: -e '$$1=="CH" { if (i>=96 && i<104){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	echo "};" ;\
	echo "static const u8 rainbow_co[] = {" ;\
	gawk -F: -e '$$1=="CO" { if (i>=96 && i<128){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	gawk -F: -e '$$1=="CO" { if (i>=96 && i<104){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	echo "};" ;\
	echo "static const u8 sprite_pat[] = {" ;\
	gawk -F: -e '$$1=="SP" { if (i<38){ print gensub(/(..)/,"0x\\1,","g",$$2) } i++ }' turmoil.mag ;\
	echo "};" ) > $@

	#LC_ALL=C gawk -F: -e '$$1=="SP" { if (i<38){ for(x=1;x<=64;x+=2) printf "%c",strtonum("0x"substr($$2,x,2)) } i++ }' turmoil.mag |../legend/tools/x86_64-Linux/dan2 | xxd -i
	#gawk -F: -e '$$1=="SP" { if (i<38){ for(x=1;x<=64;x+=2) printf "%c",strtonum("0x" substr($$2,x,2)) } i++ }' turmoil.mag | hd
	#



turmoil.rpk: turmoilc.bin layout.xml
	zip $@ $^

turmoilc.bin: turmoil.elf
	$(OBJCOPY) -O binary -j .text -j .ctors -j .data $^ $@
	ls -l $@
	@dd $(QUIET) if=/dev/null         of=$@ bs=8192 seek=1

turmoil.elf: $(PREREQUISITES)
	$(LD) $(OBJECT_LIST) $(LDFLAGS) -o $@ -Map turmoil.map --cref

turmoil.ea5: $(PREREQUISITES) linkfile.ea5
	$(LD)  crt0.o main.o --script=linkfile.ea5 -o turmoil_ea5.elf
	elf2ea5 turmoil_ea5.elf $@
	ea5split $@
	mv TURMOIL $@
	$(OBJDUMP) -t -dS turmoil_ea5.elf > turmoil_ea5.lst

turmoil.lst: turmoil.elf
	$(OBJDUMP) -t -dS $^ > turmoil.lst

turmoil.rpk: turmoilc.bin layout.xml
	zip -q $@ $^

# Recipe to clean all compiled objects
.phony clean:
	rm -f *.o
	rm -f *.elf
	rm -f *.cart

# Recipes to compile individual files
%.o: %.asm
	$(GAS) $< -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.o: %.cpp
	$(CXX) -c $< -Os -o $@

