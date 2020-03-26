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
LIB_O= abaselib.o aclslib.o astrlib.o alislib.o atablib.o amathlib.o aveclib.o aiolib.o asyslib.o amodlib.o agclib.o acorolib.o
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
	SYSCFLAGS+= -D ALOE_MACOS
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
	$(RANLIB) $(@)

$(ALO_DLL): $(BASE_O)
	$(CC) -shared -o $(@) $(BASE_O)
	$(STRIP) $(@)

$(ALO_T): $(ALO_O) $(ALO_DEPEND)
	$(CC) -o $(@) $(LDFLAGS) $(ALO_O) $(ALO_DEPEND) $(LIBS)

$(ALOC_T): $(ALOC_O) $(ALOC_DEPEND)
	$(CC) -o $(@) $(LDFLAGS) $(ALOC_O) $(ALOC_DEPEND) $(LIBS)
