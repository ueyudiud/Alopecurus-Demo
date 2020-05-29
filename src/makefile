# Alopecurus makefile.

# Users configuration.
CC= gcc -std=gnu99
RM= rm -f
AR= ar rcu
RANLIB= ranlib
STRIP= strip --strip-unneeded

LIBS= -lm $(SYSLIBS) $(USRLIBS)
CFLAGS= -O2 -Wall -Wextra $(SYSCFLAGS) $(USRCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(USRLDFLAGS)

USRLIBS=
USRCFLAGS=
USRLDFLAGS=

# Built-in configuration, don't change any things below.

# Build targets
CORE_O= aobj.o astr.o atup.o alis.o atab.o afun.o astate.o amem.o agc.o \
	abuf.o ameta.o adebug.o avm.o ado.o aeval.o achr.o aop.o acode.o alex.o \
	aparse.o aload.o asave.o aimpl.o aaux.o ainit.o
LIB_O= abaselib.o aclslib.o astrlib.o alislib.o atablib.o amathlib.o aiolib.o asyslib.o amodlib.o agclib.o acorolib.o
BASE_O= $(CORE_O) $(LIB_O)

ALO_O= alo.o
ALO_T= alo

ALOC_O= aloc.o
ALOC_T= aloc

ALO_DLL= alo.dll

ALO_A= libalo.a

ALL_O= $(BASE_O) $(ALO_O) $(ALOC_O)
ALL_T= $(ALO_T) $(ALOC_T)
ALL_A= $(ALO_A)
ALL_DLL= $(ALO_DLL)

# OS detection
ifndef (PLAT)
	ifeq ($(OS),Windows_NT)
		PLAT= WIN
	else
		ifeq ($(shell uname -s),Linux)
			PLAT= LINUX
		else
			ifeq ($(shell uname -s),Darwin)
				PLAT= OSX
			else
				PLAT= NONE
			endif
		endif
	endif
endif

# OS specified configuration
SYSLIBS=
SYSCFLAGS=
SYSLDFLAGS=

ALO_DEPEND= $(ALO_A)
ALOC_DEPEND= $(ALO_A)

ifeq ($(PLAT),WIN)
	RM:= del /f
	ALO_T:= alo.exe
	ALOC_T:= aloc.exe
	ALO_DEPEND:= $(ALO_DLL)
	SYSCFLAGS+= -D ALO_BUILD_TO_DLL
endif
ifeq ($(PLAT),LINUX)
	SYSLIBS+= -ldl
	SYSCFLAGS+= -D ALOE_LINUX
endif
ifeq ($(PLAT),OSX)
	SYSCFLAGS+= -D ALOE_MACOSX
endif
ifeq ($(PLAT),FREEBSD)
	CC:= cc -std=gnu99
	SYSCFLAGS+= -D ALOE_LINUX
endif
ifeq ($(PLAT),BSD)
	SYSCFLAGS+= -D ALO_USE_POSIX -D ALO_USE_DLOPEN
endif

# Make targets
default: all

all: $(ALL_A) $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

clean:
	$(RM) $(ALL_O) $(ALL_T) $(ALL_A) $(ALL_DLL)
	
depend:
	@$(CC) $(CFLAGS) -MM a*.c

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "AR= $(AR)"
	@echo "RM= $(RM)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"

.PHONY: all clean depend echo $(ALL_T)

# Target rules

$(ALO_A): $(BASE_O)
	$(AR) $(@) $(BASE_O)
	$(RANLIB) $(@)

$(ALO_DLL): $(BASE_O)
	$(CC) -shared -o $(@) $(BASE_O)
	$(STRIP) $(@)

$(ALO_T): $(ALO_O) $(ALO_DEPEND)
	$(CC) -o $(@) $(LDFLAGS) $(ALO_O) $(ALO_DEPEND) $(LIBS)

$(ALOC_T): $(ALOC_O) $(ALOC_DEPEND)
	$(CC) -o $(@) $(LDFLAGS) $(ALOC_O) $(ALOC_DEPEND) $(LIBS)

# AUTO GENERATED PART
aaux.o: aaux.c aaux.h alo.h adef.h acfg.h alibs.h achr.h art.h
abaselib.o: abaselib.c alo.h adef.h acfg.h aaux.h alibs.h
abuf.o: abuf.c abuf.h art.h acfg.h adef.h astate.h aobj.h ameta.h alo.h \
 amem.h adebug.h
achr.o: achr.c achr.h art.h acfg.h adef.h
aclslib.o: aclslib.c alo.h adef.h acfg.h aaux.h alibs.h
acode.o: acode.c astate.h aobj.h art.h acfg.h adef.h ameta.h alo.h agc.h \
 amem.h avm.h aeval.h alex.h abuf.h aparse.h acode.h aop.h
acorolib.o: acorolib.c alo.h adef.h acfg.h aaux.h alibs.h
adebug.o: adebug.c aop.h art.h acfg.h adef.h astr.h aobj.h astate.h \
 ameta.h alo.h abuf.h agc.h amem.h adebug.h avm.h ado.h
ado.o: ado.c astr.h aobj.h art.h acfg.h adef.h afun.h abuf.h agc.h amem.h \
 adebug.h avm.h astate.h ameta.h alo.h ado.h
aeval.o: aeval.c aop.h art.h acfg.h adef.h aobj.h astr.h atup.h alis.h \
 atab.h afun.h astate.h ameta.h alo.h agc.h amem.h abuf.h adebug.h ado.h \
 avm.h aeval.h
afun.o: afun.c astate.h aobj.h art.h acfg.h adef.h ameta.h alo.h agc.h \
 amem.h afun.h
agc.o: agc.c aop.h art.h acfg.h adef.h astr.h aobj.h atup.h atab.h afun.h \
 astate.h ameta.h alo.h abuf.h adebug.h agc.h amem.h avm.h ado.h
agclib.o: agclib.c alo.h adef.h acfg.h aaux.h alibs.h
aimpl.o: aimpl.c astr.h aobj.h art.h acfg.h adef.h atup.h alis.h atab.h \
 afun.h astate.h ameta.h alo.h abuf.h agc.h amem.h adebug.h avm.h ado.h \
 aeval.h aparse.h aload.h
ainit.o: ainit.c aaux.h alo.h adef.h acfg.h alibs.h
aiolib.o: aiolib.c alo.h adef.h acfg.h aaux.h alibs.h abuf.h art.h
alex.o: alex.c achr.h art.h acfg.h adef.h astr.h aobj.h atab.h astate.h \
 ameta.h alo.h agc.h amem.h abuf.h adebug.h avm.h ado.h alex.h aparse.h
alis.o: alis.c alis.h aobj.h art.h acfg.h adef.h agc.h amem.h adebug.h \
 avm.h astate.h ameta.h alo.h
alislib.o: alislib.c alo.h adef.h acfg.h aaux.h alibs.h
alo.o: alo.c alo.h adef.h acfg.h aaux.h alibs.h
aload.o: aload.c astr.h aobj.h art.h acfg.h adef.h alis.h afun.h astate.h \
 ameta.h alo.h agc.h amem.h avm.h ado.h aload.h abuf.h
aloc.o: aloc.c acfg.h aall.h adef.h art.h aop.h achr.h aobj.h astr.h \
 atup.h alis.h atab.h afun.h astate.h ameta.h alo.h amem.h agc.h abuf.h \
 adebug.h avm.h ado.h alex.h aparse.h aload.h aaux.h alibs.h
amathlib.o: amathlib.c alo.h adef.h acfg.h aaux.h alibs.h
amem.o: amem.c astate.h aobj.h art.h acfg.h adef.h ameta.h alo.h adebug.h \
 amem.h agc.h ado.h
ameta.o: ameta.c astr.h aobj.h art.h acfg.h adef.h alis.h atab.h ameta.h \
 agc.h amem.h adebug.h avm.h astate.h alo.h ado.h
amodlib.o: amodlib.c alo.h adef.h acfg.h aaux.h alibs.h
aobj.o: aobj.c achr.h art.h acfg.h adef.h aobj.h astr.h atup.h alis.h \
 atab.h ameta.h ado.h astate.h alo.h adebug.h
aop.o: aop.c aop.h art.h acfg.h adef.h
aparse.o: aparse.c aop.h art.h acfg.h adef.h astr.h aobj.h afun.h agc.h \
 amem.h avm.h astate.h ameta.h alo.h ado.h alex.h abuf.h aparse.h acode.h \
 aload.h
asave.o: asave.c astr.h aobj.h art.h acfg.h adef.h astate.h ameta.h alo.h \
 aload.h abuf.h
astate.o: astate.c astr.h aobj.h art.h acfg.h adef.h atab.h afun.h \
 astate.h ameta.h alo.h adebug.h agc.h amem.h ado.h alex.h abuf.h \
 aparse.h
astr.o: astr.c astr.h aobj.h art.h acfg.h adef.h astate.h ameta.h alo.h \
 agc.h amem.h
astrlib.o: astrlib.c alo.h adef.h acfg.h aaux.h alibs.h
asyslib.o: asyslib.c alo.h adef.h acfg.h aaux.h alibs.h
atab.o: atab.c astr.h aobj.h art.h acfg.h adef.h atab.h agc.h amem.h \
 adebug.h avm.h astate.h ameta.h alo.h ado.h
atablib.o: atablib.c alo.h adef.h acfg.h aaux.h alibs.h
atup.o: atup.c atup.h aobj.h art.h acfg.h adef.h agc.h amem.h adebug.h \
 avm.h astate.h ameta.h alo.h
avm.o: avm.c aop.h art.h acfg.h adef.h astr.h aobj.h atup.h alis.h atab.h \
 afun.h abuf.h ameta.h agc.h amem.h adebug.h avm.h astate.h alo.h ado.h \
 aeval.h

# End of Makefile
