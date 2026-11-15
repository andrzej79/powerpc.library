# Cross-build of powerpc.library 17.15 with vbcc. See README.md.
#
#   make            build the toolchain if needed, then build/powerpc.library
#   make verify     compare the result with the shipped library
#   make clean      remove build/
#   make distclean  also remove toolchain/
#
# Tested on macOS (Apple silicon). Paths can be overridden on the command line.

VBCC     ?= /opt/amiga_sdk/vbcc
VBCC_SRC ?= /opt/amiga_sdk/download/vbcc-2022-11-01
NDK      ?= /opt/amiga_sdk/NDK3.2R4
SRC      ?= ..
# make verify: the library to compare with (e.g. the one an Amiga runs)
SHIPPED  ?= ../../a3000/LIBS/PowerPC/powerpc.library
# the WarpOS C headers: the installed vbcc ppc-warpos target's
WOS_C_INC ?= $(VBCC)/targets/ppc-warpos/include

B := build
T := toolchain

# --- Tools -----------------------------------------------------------------
VBCCM68K := $(VBCC)/bin/vbccm68k
VASM68K  := $(VBCC)/bin/vasmm68k_mot
VLINK    := $(VBCC)/bin/vlink
VASMPPC  := $(T)/vasm/vasmppc_std
VBCCPPC  := $(T)/vbcc/bin/vbccppc
VSCPPC   := $(T)/vbcc/bin/vscppc
PPCTOOLS := $(VASMPPC) $(VBCCPPC) $(VSCPPC)

# --- Flags -----------------------------------------------------------------
# -O=1023 is what "vc -O2" passes down. On PPC, vc also runs the vscppc scheduler.
INCS      := -I$(SRC) -Iinclude -Ithird_party/warpos
PPCINCS   := $(INCS) -I$(WOS_C_INC) -I$(NDK)/Include_H
M68KINCS  := $(INCS) -I$(VBCC)/targets/m68k-amigaos/include -I$(NDK)/Include_H
PPCCFLAGS := -quiet -poweropen -sc -no-multiple-ccs -madd -c99 -O=1023 -speed
M68KCFLAGS:= -quiet -cpu=68030 -sc -cpp-comments -c99 -O=1023 -speed
ASPPC     := -quiet -many -mppc32 -mavec -Fhunk -nowarn=62
ASPPCC    := $(ASPPC) -sdreg=2 -opt-branch
AS68K     := -quiet -Fhunk -nowarn=62

HEADERS := $(wildcard $(SRC)/*.h $(SRC)/*.i include/*/*)

# Link order is a contract. LibInit finds setup, kernel and functions by walking
# the seglist, and main() in libinit.c must come first. See README.md.
OBJS := $(B)/libinit.o $(B)/functions68kasm.o $(B)/functions68k.o $(B)/support68k.o \
        $(B)/setupppcasm.o $(B)/setupppc.o $(B)/kernelasm.o $(B)/kernel.o \
        $(B)/functionsppc.o $(B)/supportppcasm.o $(B)/supportppc.o

.PHONY: all verify toolchain clean distclean
.SECONDARY:
.DELETE_ON_ERROR:

all: $(B)/powerpc.library

$(B)/powerpc.library: $(OBJS)
	$(VLINK) -bamigahunk -mrel -s -Rshort -Bstatic -nostdlib -Cvbcc $(OBJS) -o $@
	@echo "Built $@"

# --- 68k -------------------------------------------------------------------
$(B)/libinit.o $(B)/functions68k.o $(B)/support68k.o: $(B)/%.o: $(SRC)/%.c $(HEADERS) | $(B)
	$(VBCCM68K) $(M68KCFLAGS) $(M68KINCS) $< -o=$(B)/$*.asm
	$(VASM68K) $(AS68K) -o $@ $(B)/$*.asm

$(B)/functions68kasm.o: $(SRC)/functions68k.s $(HEADERS) | $(B)
	$(VASM68K) $(AS68K) -m68020up -I$(NDK)/Include_I -Iinclude -o $@ $<

# --- PowerPC ---------------------------------------------------------------
$(B)/setupppc.o $(B)/kernel.o $(B)/functionsppc.o $(B)/supportppc.o: $(B)/%.o: $(SRC)/%.c $(HEADERS) | $(B) $(PPCTOOLS)
	$(VBCCPPC) $(PPCCFLAGS) $(PPCINCS) $< -o=$(B)/$*.asm
	$(VSCPPC) -quiet $(B)/$*.asm $(B)/$*.sch
	$(VASMPPC) $(ASPPCC) -o $@ $(B)/$*.sch

$(B)/kernelasm.o $(B)/supportppcasm.o: $(B)/%asm.o: $(SRC)/%.s $(HEADERS) | $(B) $(PPCTOOLS)
	$(VASMPPC) $(ASPPC) -I$(SRC) -o $@ $<

# vasm 1.9a reads 0b1110 as a reference to local label 0, so the PLL tables are
# rewritten in hex on the way through. The source file is not changed.
$(B)/setupppc_hex.s: $(SRC)/setupppc.s | $(B)
	perl -pe 's/\b0b([01]+)/sprintf("0x%x",oct("0b$$1"))/ge' $< > $@

$(B)/setupppcasm.o: $(B)/setupppc_hex.s $(HEADERS) | $(B) $(PPCTOOLS)
	$(VASMPPC) $(ASPPC) -I$(SRC) -o $@ $<

$(B):
	mkdir -p $@

# --- Toolchain -------------------------------------------------------------
# Private PPC tools, built here from the vbcc sources. /opt is not modified.
#
# JUMP_TABLE_LENGTH is raised because vbcc 0.9h compiles a long dense switch to
# a table of absolute addresses. The PPC hunks are copied to the card without
# being relocated, so they must not contain absolute references.
#
# The answers piped in are for dtgen's questions about the host's data types.
DTGEN_ANSWERS := y\ny\nsigned char\ny\nunsigned char\nn\ny\nsigned short\nn\ny\nunsigned short\nn\ny\nsigned int\nn\ny\nunsigned int\nn\ny\nsigned long long\nn\ny\nunsigned long long\nn\ny\nfloat\nn\ny\ndouble\n

toolchain: $(PPCTOOLS)

$(VASMPPC):
	mkdir -p $(T)
	rm -rf $(T)/vasm
	cp -R $(VBCC_SRC)/vasm $(T)/vasm
	rm -f $(T)/vasm/obj/*.o
	$(MAKE) -s -C $(T)/vasm CPU=ppc SYNTAX=std 2>/dev/null

$(T)/vbcc/.patched:
	mkdir -p $(T)
	rm -rf $(T)/vbcc
	cp -R $(VBCC_SRC)/vbcc $(T)/vbcc
	mkdir -p $(T)/vbcc/objects/ppc
	perl -pi -e 's/^#define JUMP_TABLE_LENGTH 12$$/#define JUMP_TABLE_LENGTH 100000/' $(T)/vbcc/machines/ppc/machine.h
	grep -q 'JUMP_TABLE_LENGTH 100000' $(T)/vbcc/machines/ppc/machine.h
	touch $@

$(VBCCPPC): $(T)/vbcc/.patched
	printf '$(DTGEN_ANSWERS)' | $(MAKE) -s -C $(T)/vbcc TARGET=ppc bin/vbccppc >/dev/null 2>&1

$(VSCPPC): $(VBCCPPC)
	$(MAKE) -s -C $(T)/vbcc TARGET=ppc bin/vscppc >/dev/null 2>&1

# --- Checks ----------------------------------------------------------------
verify: $(B)/powerpc.library
	python3 -I tools/compare_hunks.py $(SHIPPED) $<

clean:
	rm -rf $(B)

distclean: clean
	rm -rf $(T)
