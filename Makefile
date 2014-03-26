version := 2.0.a12

system	?= $(shell uname -s)

ifeq ($(system), Darwin)
LIB_EXT = dylib
else
LIB_EXT = so
endif

DESTDIR ?= 
PREFIX ?= /usr/local
CROSS=i586-mingw32msvc-

MAKEFILE := Makefile.unix

prefix := $(DESTDIR)$(PREFIX)
arch   := $(wildcard architecture/*.*)
mfiles := $(wildcard examples/Makefile.*)
vname := faust-$(version)-$(shell date +%y%m%d.%H%M%S)
zname := faust-$(version)

all :
	$(MAKE) -C compiler -f $(MAKEFILE) prefix=$(prefix)
	$(MAKE) -C architecture/osclib
	

dynamic :
	$(MAKE) -C compiler -f $(MAKEFILE) dynamic prefix=$(prefix)
	$(MAKE) -C architecture/httpdlib/src dynamic

httpd :
	$(MAKE) -C architecture/httpdlib/src all

remote :
	$(MAKE) -C embedded/faustremote/RemoteServer all
	$(MAKE) -C embedded/faustremote/RemoteClient all

win32 :
	$(MAKE) -C compiler -f $(MAKEFILE) prefix=$(prefix) CXX=$(CROSS)g++
	$(MAKE) -C architecture/osclib CXX=$(CROSS)g++ system=Win32

ios :
	$(MAKE) -C compiler ios -f $(MAKEFILE) prefix=$(prefix) 

sound2faust: 

	$(MAKE) -C tools/sound2faust

.PHONY: clean depend install uninstall dist parser help

help :
	@echo "Usage : 'make; sudo make install'"
	@echo "For http support : 'make httpd; make; sudo make install' (requires GNU libmicrohttpd)"
	@echo "make or make all : compile the faust compiler"
	@echo "make httpd : compile httpdlib (requires GNU libmicrohttpd)"
	@echo "make sound2faust : compile sound to DSP file converter"
	@echo "make parser : generate the parser from the lex and yacc files"
	@echo "make clean : remove all object files"
	@echo "make doc : generate the documentation using doxygen"
	@echo "make install : install the compiler, tools and the architecture files in $(prefix)/bin $(prefix)/lib/faust $(prefix)/include/faust"
	@echo "make uninstall : undo what install did"
	@echo "make dist : make a Faust distribution as a .zip file"
	@echo "make log : make a changelog file"

parser :
	$(MAKE) -C compiler -f $(MAKEFILE) parser

clean :
	$(MAKE) -C compiler -f $(MAKEFILE) clean
	$(MAKE) -C examples clean
	$(MAKE) -C architecture/osclib clean
	$(MAKE) -C architecture/httpdlib/src clean
	$(MAKE) -C embedded/faustremote/RemoteServer clean
	$(MAKE) -C embedded/faustremote/RemoteClient clean

depend :
	$(MAKE) -C compiler -f $(MAKEFILE) depend
	$(MAKE) -C architecture/osclib depend
	$(MAKE) -C architecture/httpdlib/src depend


doc :
	$(MAKE) -C compiler -f $(MAKEFILE) doc


install :
	# install faust itself
	mkdir -p $(prefix)/bin/
	mkdir -p $(prefix)/include/
	mkdir -p $(prefix)/include/faust/
	mkdir -p $(prefix)/include/faust/osc/
	mkdir -p $(prefix)/lib/faust
	install compiler/faust $(prefix)/bin/
	install compiler/libfaust.a $(prefix)/lib/faust
	([ -e compiler/libfaust.$(LIB_EXT) ] && install compiler/libfaust.$(LIB_EXT) $(prefix)/lib/faust) || echo libfaust.$(LIB_EXT) not available
	cp compiler/libfaust.h  $(prefix)/include/faust/
	cp compiler/generator/llvm-dsp.h  $(prefix)/include/faust/
	cp compiler/generator/llvm-c-dsp.h  $(prefix)/include/faust/
	chmod gou+r compiler/scheduler.ll
	cp compiler/scheduler.ll  $(prefix)/lib/faust
	
	# install architecture and faust library files
	cp architecture/*.c $(prefix)/lib/faust/
	cp architecture/*.cpp $(prefix)/lib/faust/
	cp architecture/*.java $(prefix)/lib/faust/
	cp architecture/*.js $(prefix)/lib/faust/
	cp architecture/*.html $(prefix)/lib/faust/
	cp architecture/*.lib $(prefix)/lib/faust/
	# install iOS
	rm -rf $(prefix)/lib/faust/iOS
	cp -r architecture/iOS $(prefix)/lib/faust/
	rm -rf $(prefix)/lib/faust/iOS/DerivedData/
	# install AU
	rm -rf $(prefix)/lib/faust/AU/
	cp -r architecture/AU $(prefix)/lib/faust/
	cp -r architecture/android $(prefix)/lib/faust/
	# install math documentation files
	cp architecture/mathdoctexts-*.txt $(prefix)/lib/faust/
	cp architecture/latexheader.tex $(prefix)/lib/faust/
	# install additional binary libraries (osc, http,...)
	([ -e architecture/httpdlib/libHTTPDFaust.a ] && cp architecture/httpdlib/libHTTPDFaust.a $(prefix)/lib/faust/) || echo libHTTPDFaust not available
	([ -e architecture/httpdlib/libHTTPDFaust.$(LIB_EXT) ] && cp architecture/httpdlib/libHTTPDFaust.$(LIB_EXT) $(prefix)/lib/faust/) || echo libHTTPDFaust not available	
	cp architecture/osclib/*.a $(prefix)/lib/faust/
	# install includes files for architectures
	cp -r architecture/faust $(prefix)/include/
	# install additional includes files for binary libraries  (osc, http,...)
	cp architecture/osclib/faust/faust/OSCControler.h $(prefix)/include/faust/gui/
	cp architecture/osclib/faust/faust/osc/*.h $(prefix)/include/faust/osc/
	cp architecture/httpdlib/src/include/*.h $(prefix)/include/faust/gui/
	# install faust2xxx tools
	make -C tools/faust2appls install
	# install sound converter
	[ -e tools/sound2faust/sound2faust ] && make -C tools/sound2faust install || echo sound2faust not compiled	
	#install faustremote
	([ -e embedded/faustremote/RemoteClient/libfaustremote.a ] &&  install embedded/faustremote/RemoteClient/libfaustremote.a  $(prefix)/lib/faust/) || echo remote not compiled
	([ -e embedded/faustremote/RemoteServer/RemoteServer ] &&  install embedded/faustremote/RemoteServer/RemoteServer  $(prefix)/bin) || echo remote not compiled
	cp embedded/faustremote/RemoteClient/remote-dsp.h  $(prefix)/include/faust/

uninstall :
	rm -rf $(prefix)/lib/faust/
	rm -rf $(prefix)/include/faust/
	rm -f $(prefix)/bin/faust
	make -C tools/faust2appls uninstall

# make a faust distribution .zip file
dist :
	git archive -o faust-$(version).zip --prefix=faust-$(version)/ HEAD


log :
	git log --oneline --date-order --reverse --after={2011-01-07} master >log-$(version)
	
# DO NOT DELETE
