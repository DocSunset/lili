include config.mk

all: options lili.c lili

options:
	@echo lili build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

lili.c: lili.lili
	@echo tangling source from $^
	@lili lili.lili

${OBJ}: config.mk

lili: lili.c
	@echo building $@
	@${CC} ${CFLAGS} ${LDFLAGS} -o $@ lili.c

lili.debug: lili.c
	@echo building $@
	@${CC} ${DEBUGFLAGS} ${LDFLAGS} -o $@ lili.c

clean:
	@echo cleaning
	@rm -f lili lili.debug ${OBJ} ${LIBOBJ} lili-${VERSION}.tar.gz

dist: clean lili.c
	@echo creating dist tarball
	@mkdir -p lili-${VERSION}
	@cp -R Makefile config.mk lili.c lili.lili LICENSE lili-${VERSION}
	@tar -cf lili-${VERSION}.tar lili-${VERSION}
	@gzip lili-${VERSION}.tar
	@rm -rf lili-${VERSION}

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f lili ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/lili

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/lili

test_makes_file: lili
	@mv lili.c lili.bak
	@echo test lili produces lili.c
	@./lili lili.lili
	@test -f lili.c
	@echo success

test_same_result: lili
	@echo test lili produces the same result every time
	@mv lili.c lili.bak
	@./lili lili.lili
	@mv lili.c lili.c1
	@./lili lili.lili
	@cmp lili.c lili.c1
	@echo success

test_agrees_with_installed: lili
	@echo test ./lili produces same result as system lili
	@mv lili.c lili.bak
	@./lili lili.lili
	@mv lili.c lili.new
	@lili lili.lili
	@cmp lili.c lili.new
	@echo success

test_indents: lili
	@echo test ./lili produces correct indents
	@./lili test/indents.lili
	@cmp indents.out indents.expect
	@echo success

test_single_invocations: lili
	@echo test ./lili ignores multiple invocations
	-./lili test/single_invocations.lili ; test $$? != 0 && echo success || echo failure

test_tangle_invocations: lili
	@echo test ./lili ignores tangle invocations
	-./lili test/tangle_invocations.lili ; test $$? != 0 && echo success || echo failure

test: test_makes_file test_same_result test_agrees_with_installed test_indents test_single_invocations test_tangle_invocations
	@echo ran all tests
	@mv lili.bak lili.c
	@[ -f lili.new ] && rm lili.new
	@[ -f lili.c1 ] && rm lili.c1
	@rm -f *.out* *.expect*

.PHONY: all options clean dist install uninstall test test_makes_file test_same_result test_agrees_with_installed
