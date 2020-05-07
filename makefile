# Makefile for installing Alopecurus
# Use makefile_shl in shell or makefile_cmd in command line
# Change the settings to suit your environment.

export V= 0.2

ifndef (SHELL)
	ifeq ($(OS),Windows_NT)
		MAKE_SUB= "$(MAKE)" -f cmd.mk
		
	else
		MAKE_SUB= "$(MAKE)" -f shl.mk
	endif
endif

default: build

build:
	cd src && "$(MAKE)" all

test: build
	src/alo -v

clean:
	cd src && "$(MAKE)" clean

install: build
	$(MAKE_SUB) install

uninstall:
	$(MAKE_SUB) uninstall

.PHONY: default build clean install uninstall
