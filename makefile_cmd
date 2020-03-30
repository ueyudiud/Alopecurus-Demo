# Makefile for installing Alopecurus
# cmd environment detected

INSTALL_ROOT= C:\Program Files\Alopecurus\$V
INSTALL_BIN= $(INSTALL_ROOT)\bin
INSTALL_INC= $(INSTALL_ROOT)\include
INSTALL_LIB= $(INSTALL_ROOT)\lib
INSTALL_LMOD= $(INSTALL_ROOT)\mod\alo
INSTALL_CMOD= $(INSTALL_ROOT)\mod\C

MKDIR= md
RM= del /f

INSTALL= copy /Y
INSTALL_EXE= $(INSTALL)
INSTALL_DAT= $(INSTALL)

TO_BIN= alo.exe aloc.exe alo.dll
TO_INC= adef.h acfg.h alo.h aaux.h alibs.h alo.hpp
TO_LIB= libalo.a

INSTALL_DIRS= "$(INSTALL_BIN)" "$(INSTALL_INC)" "$(INSTALL_LIB)" "$(INSTALL_LMOD)" "$(INSTALL_CMOD)"

install:
	cd src && set INSTALL_DIRS= $(INSTALL_DIRS) && for %%d in (%INSTALL_DIRS%) do (if not exist %%d ($(MKDIR) %%d))
	cd src && for %%f in ($(TO_BIN)) do $(INSTALL_EXE) %%f "$(INSTALL_BIN)"
	cd src && for %%f in ($(TO_INC)) do $(INSTALL_DAT) %%f "$(INSTALL_INC)"
	cd src && for %%f in ($(TO_LIB)) do $(INSTALL_DAT) %%f "$(INSTALL_LIB)"

uninstall:
	cd src && cd "$(INSTALL_BIN)" && $(RM) $(TO_BIN)
	cd src && cd "$(INSTALL_INC)" && $(RM) $(TO_INC)
	cd src && cd "$(INSTALL_LIB)" && $(RM) $(TO_LIB)

.PHONY: default build clean install uninstall
