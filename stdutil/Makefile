stdutil: src/Makefile
	cd src; make

src/Makefile:
	cd src; ./configure

clean: src/Makefile
	cd src; make clean

distclean: src/Makefile
	rm -f config.* *~; cd src; make distclean
