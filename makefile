# Makefile for installing Alopecurus
# Use makefile_shl in shell or makefile_cmd in command line
# Change the settings to suit your environment.

export V= 0.2

ifndef (SHELL)
	ifeq ($(OS),Windows_NT)
		MAKE_SUB= $(MAKE) -f makefile_cmd
		
	else
		MAKE_SUB= $(MAKE) -f makefile_shl
	endif
endif

default:  build

build:
	cd src && $(MAKE) all

clean:
	cd src && $(MAKE) clean

install: build
	$(MAKE_SUB) install

uninstall: build
	$(MAKE_SUB) uninstall

.PHONY: default build clean install uninstall
