# Alopecurus makefile.

CC= gcc
CFLAGS= -O2 -Wall
RM= del /f
LIBS= -lm

CORE_O= aop.o aobj.o astr.o atup.o alis.o atab.o afun.o astate.o \
	amem.o agc.o abuf.o ameta.o adebug.o avm.o ado.o aeval.o achr.o \
	acode.o alex.o aparse.o aload.o asave.o aimpl.o aaux.o ainit.o
LIB_O= abaselib.o aclslib.o astrlib.o alislib.o atablib.o amathlib.o aveclib.o aiolib.o

ALO_O= alo.o
ALO_T= alo.exe

ALL_O= $(CORE_O) $(LIB_O) $(ALO_O)
ALL_T= $(ALO_T)
ALL_A= 

all: $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

$(ALO_T): $(ALO_O) $(LIB_O) $(CORE_O)
	$(CC) $(CFLAGS) -o $(@) $(ALO_O) $(LIB_O) $(CORE_O) $(LIBS)

clean:
	$(RM) $(ALL_O) $(ALL_T) $(ALL_A)

echo:
	@echo "CC= $(CC)"
	@echo "RM= $(RM)"
	@echo "CFLAGS= $(CFLAGS)"

.PHONY: all clean echo $(ALL_T)
