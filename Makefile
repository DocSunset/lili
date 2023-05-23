include config.mk

all: options litlit.c litlit

options:
	@echo litlit build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

litlit.c: litlit.litlit
	@echo tangling source from $^
	@litlit litlit.litlit

${OBJ}: config.mk

litlit: litlit.c
	@echo building $@
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ litlit.c

litlit.debug: litlit.c
	@echo building $@
	@${CC} ${DEBUGFLAGS} ${LDFLAGS} -o $@ litlit.c

clean:
	@echo cleaning
	@rm -f litlit litlit.debug ${OBJ} ${LIBOBJ} litlit-${VERSION}.tar.gz

dist: clean litlit.c
	@echo creating dist tarball
	@mkdir -p litlit-${VERSION}
	@cp -R Makefile config.mk litlit.c litlit.litlit LICENSE litlit-${VERSION}
	@tar -cf litlit-${VERSION}.tar litlit-${VERSION}
	@gzip litlit-${VERSION}.tar
	@rm -rf litlit-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f litlit ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/litlit

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/litlit

test_makes_file: litlit
	@mv litlit.c litlit.bak
	@echo test litlit produces litlit.c
	@./litlit litlit.litlit
	@test -f litlit.c
	@echo success

test_same_result: litlit
	@echo test litlit produces the same result every time
	@mv litlit.c litlit.bak
	@./litlit litlit.litlit
	@mv litlit.c litlit.c1
	@./litlit litlit.litlit
	@cmp litlit.c litlit.c1
	@echo success

test_agrees_with_installed: litlit
	@echo test ./litlit produces same result as system litlit
	@mv litlit.c litlit.bak
	@./litlit litlit.litlit
	@mv litlit.c litlit.new
	@litlit litlit.litlit
	@cmp litlit.c litlit.new
	@echo success

test_indents: litlit
	@echo test ./litlit produces correct indents
	@./litlit test/indents.litlit
	@cmp indents.out indents.expect
	@echo success

test_single_invocations: litlit
	@echo test ./litlit ignores multiple invocations
	@./litlit test/single_invocations.litlit
	@cmp single_invocations.out single_invocations.expect
	@echo success

test_tangle_invocations: litlit
	@echo test ./litlit ignores tangle invocations
	@./litlit test/tangle_invocations.litlit
	@cmp tangle_invocations.out2 tangle_invocations.expect2
	@echo success

test: test_makes_file test_same_result test_agrees_with_installed test_indents test_single_invocations test_tangle_invocations
	@echo ran all tests
	@mv litlit.bak litlit.c
	@[ -f litlit.new ] && rm litlit.new
	@[ -f litlit.c1 ] && rm litlit.c1
	@rm -f *.out* *.expect*

.PHONY: all options clean dist install uninstall test test_makes_file test_same_result test_agrees_with_installed
