# Alopecurus makefile.

# Users configuration.
CC= gcc -std=gnu99
RM= rm -f
AR= ar -rcu
STRIP= strip --strip-unneeded

LIBS= -lm $(SYSLIBS) $(USRLIBS)
CFLAGS= -O2 -Wall $(SYSCFLAGS) $(USRCFLAGS)
LDFLAGS= $(SYSLDFLAGS) $(USRLDFLAGS)

USRLIBS=
USRCFLAGS=
USRLDFLAGS=

# Built-in configuration, don't change any things below.

# Build targets
CORE_O= aobj.o astr.o atup.o alis.o atab.o afun.o astate.o amem.o agc.o \
	abuf.o ameta.o adebug.o avm.o ado.o aeval.o achr.o aop.o acode.o alex.o \
	aparse.o aload.o asave.o aimpl.o aaux.o ainit.o
LIB_O= abaselib.o aclslib.o astrlib.o alislib.o atablib.o amathlib.o aveclib.o aiolib.o asyslib.o amodlib.o agclib.o acorolib.o
BASE_O= $(CORE_O) $(LIB_O)

ALO_O= alo.o
ALO_T= alo

ALOC_O= aloc.o
ALOC_T= aloc

ALO_A= libalo.a

ALO_SO= libalo.so

ALL_O= $(BASE_O) $(ALO_O) $(ALOC_O)
ALL_T= $(ALO_T) $(ALOC_T)
ALL_A= $(ALO_A)
ALL_SO= $(ALO_SO)

# OS detection
ifndef (TARGET_OS)
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

ifeq ($(PLAT),WIN)
	RM:= del /f
	ALO_T:= alo.exe
	ALOC_T:= aloc.exe
	ALO_SO:= alo.dll
	SYSCFLAGS+= -D ALO_BUILD_TO_DLL
else
	ifeq ($(PLAT),LINUX)
		SYSLIBS+= -ldl
		SYSCFLAGS+= -D ALOE_LINUX
	endif
	ifeq ($(PLAT),OSX)
		SYSCFLAGS+= -D ALOE_MACOS
	endif
endif

# Make targets
default: all

all: $(ALL_A) $(ALL_SO) $(ALL_T)

o: $(ALL_O)

a: $(ALL_A)

clean:
	$(RM) $(ALL_O) $(ALL_T) $(ALL_A) $(ALL_SO)

echo:
	@echo "PLAT= $(PLAT)"
	@echo "CC= $(CC)"
	@echo "AR= $(AR)"
	@echo "RM= $(RM)"
	@echo "CFLAGS= $(CFLAGS)"
	@echo "LDFLAGS= $(LDFLAGS)"
	@echo "LIBS= $(LIBS)"

.PHONY: all clean echo $(ALL_T)

# Target rules

$(ALO_A): $(BASE_O)
	$(AR) $(@) $(BASE_O)

$(ALO_SO): $(BASE_O)
	$(CC) -shared -o $(@) $(BASE_O)
	$(STRIP) $(@)

$(ALO_T): $(ALO_O) $(ALO_SO)
	$(CC) -o $(@) $(LDFLAGS) $(ALO_O) $(ALO_SO) $(LIBS)

$(ALOC_T): $(ALOC_O) $(ALO_A)
	$(CC) -o $(@) $(LDFLAGS) $(ALOC_O) $(ALO_A) $(LIBS)

