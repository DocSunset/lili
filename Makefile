include config.mk

all: options lilit.c lilit

options:
	@echo lilit build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

lilit.c: lilit.lilit
	@echo tangling source from $^
	@lilit lilit.lilit

${OBJ}: config.mk

lilit: lilit.c
	@echo building $@
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ lilit.c

lilit.debug: lilit.c
	@echo building $@
	@${CC} ${DEBUGFLAGS} ${LDFLAGS} -o $@ lilit.c

clean:
	@echo cleaning
	@rm -f lilit lilit.debug ${OBJ} ${LIBOBJ} lilit-${VERSION}.tar.gz

dist: clean lilit.c
	@echo creating dist tarball
	@mkdir -p lilit-${VERSION}
	@cp -R Makefile config.mk lilit.c lilit.lilit LICENSE lilit-${VERSION}
	@tar -cf lilit-${VERSION}.tar lilit-${VERSION}
	@gzip lilit-${VERSION}.tar
	@rm -rf lilit-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f lilit ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/lilit

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/lilit

test_makes_file: lilit
	@mv lilit.c lilit.bak
	@echo test lilit produces lilit.c
	@./lilit lilit.lilit
	@test -f lilit.c
	@echo success

test_same_result: lilit
	@echo test lilit produces the same result every time
	@mv lilit.c lilit.bak
	@./lilit lilit.lilit
	@mv lilit.c lilit.c1
	@./lilit lilit.lilit
	@cmp lilit.c lilit.c1
	@echo success

test_agrees_with_installed: lilit
	@echo test ./lilit produces same result as system lilit
	@mv lilit.c lilit.bak
	@./lilit lilit.lilit
	@mv lilit.c lilit.new
	@lilit lilit.lilit
	@cmp lilit.c lilit.new
	@echo success

test_indents: lilit
	@echo test ./lilit produces correct indents
	@./lilit test/indents.lilit
	@cmp indents.out indents.expect
	@echo success

test_multifile: lilit
	@echo test ./lilit recurses files with @[]
	@./lilit test/multifile.lilit
	@cmp multifile.out multifile.expect
	@echo success

test: test_makes_file test_same_result test_agrees_with_installed test_indents test_multifile
	@echo ran all tests
	@mv lilit.bak lilit.c
	@[ -f lilit.new ] && rm lilit.new
	@[ -f lilit.c1 ] && rm lilit.c1
	@rm -f *.out *.expect

.PHONY: all options clean dist install uninstall test test_makes_file test_same_result test_agrees_with_installed
