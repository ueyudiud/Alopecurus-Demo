# Makefile for installing Alopecurus
# shell environment detected

INSTALL_ROOT= /usr/local
INSTALL_BIN= $(INSTALL_ROOT)/bin
INSTALL_INC= $(INSTALL_ROOT)/include
INSTALL_LIB= $(INSTALL_ROOT)/lib
INSTALL_LMOD= $(INSTALL_ROOT)/share/alo/$V
INSTALL_CMOD= $(INSTALL_ROOT)/lib/alo/$V

MKDIR= mkdir -p
RM= rm -f

ifeq ($(shell command -v install),install)
	INSTALL= install -p
	INSTALL_EXE= $(INSTALL) -m 0755
	INSTALL_DAT= $(INSTALL) -m 0644
else
	INSTALL= cp -p
	INSTALL_EXE= $(INSTALL)
	INSTALL_DAT= $(INSTALL)
endif

TO_BIN= alo aloc
TO_INC= adef.h acfg.h alo.h aaux.h alibs.h alo.hpp
TO_LIB= libalo.a

ifeq ($(PLAT),win)
	TO_BIN:= alo.exe aloc.exe alo.dll
	TO_LIB:= alo.dll
endif

install:
	cd src && $(MKDIR) "$(INSTALL_BIN)" "$(INSTALL_INC)" "$(INSTALL_LIB)" "$(INSTALL_LMOD)" "$(INSTALL_CMOD)"
	cd src && $(INSTALL_EXE) $(TO_BIN) "$(INSTALL_BIN)"
	cd src && $(INSTALL_DAT) $(TO_INC) "$(INSTALL_INC)"
	cd src && $(INSTALL_DAT) $(TO_LIB) "$(INSTALL_LIB)"

uninstall:
	cd src && cd "$(INSTALL_BIN)" && $(RM) $(TO_BIN)
	cd src && cd "$(INSTALL_INC)" && $(RM) $(TO_INC)
	cd src && cd "$(INSTALL_LIB)" && $(RM) $(TO_LIB)

.PHONY: default build clean install uninstall
