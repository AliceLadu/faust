version := 0.9.9.3l-par
prefix := /usr/local
arch   := $(wildcard architecture/*.*)
mfiles := $(wildcard examples/Makefile.*)
vname := faust-$(version)-$(shell date +%y%m%d.%H%M%S)


all :
	$(MAKE) -C compiler


.PHONY: clean depend install ininstall dist parser help

help :
	@echo "make or make all : compiler the faust compiler"
	@echo "make parser : generate the parser from the lex and yacc files"
	@echo "make clean : remove all object files"
	@echo "make doc : generate the documentation using doxygen"
	@echo "make install : install the compiler and the architecture files in $(prefix)/bin $(prefix)/lib/faust"
	@echo "make uninstall : undo what install did"
	@echo "make dist : make a tar.gz file ready for distribution"
	@echo "make log : make a changelog file"

parser :
	$(MAKE) -C compiler parser

clean :
	$(MAKE) -C compiler clean

depend :
	$(MAKE) -C compiler depend


doc :
	$(MAKE) -C compiler doc


install :
	mkdir -p $(prefix)/lib/faust/
	install compiler/faust $(prefix)/bin
	install -m 0644 $(arch) $(prefix)/lib/faust/
	- test -d  $(prefix)/lib/faust/VST && rm -rf $(prefix)/lib/faust/VST
	cp -r architecture/VST $(prefix)/lib/faust/
	find $(prefix)/lib/faust/ -name CVS | xargs rm -rf
	install -m 0644 $(mfiles) $(prefix)/lib/faust/


uninstall :
	rm -rf $(prefix)/lib/faust/
	rm -f $(prefix)/bin/faust

dist :
	$(MAKE) -C compiler clean
	$(MAKE) -C examples clean
	mkdir -p faust-$(version)
	cp README COPYING Makefile faust-$(version)
	cp -r architecture faust-$(version)
	cp -r compiler faust-$(version)
	cp -r examples faust-$(version)
	cp -r documentation faust-$(version)
	cp -r syntax-highlighting faust-$(version)
	cp -r tools faust-$(version)
	cp -r windows faust-$(version)
	find faust-$(version) -name CVS | xargs rm -rf
	find faust-$(version) -name "*~" | xargs rm -rf
	find faust-$(version) -name ".#*" | xargs rm -rf
	rm -f faust-$(version).tar.gz
	tar czfv faust-$(version).tar.gz faust-$(version)
	rm -rf faust-$(version)

archive :
	$(MAKE) -C compiler clean
	$(MAKE) -C examples clean
	mkdir -p $(vname)
	cp README COPYING Makefile $(vname)
	cp -r architecture $(vname)
	cp -r compiler $(vname)
	cp -r examples $(vname)
	cp -r documentation $(vname)
	cp -r syntax-highlighting $(vname)
	cp -r tools $(vname)
	cp -r windows $(vname)
	find $(vname) -name "*~" | xargs rm -rf
	tar czfv $(vname).tar.gz $(vname)
	rm -rf $(vname)

log :
	cvs2cl --fsf
