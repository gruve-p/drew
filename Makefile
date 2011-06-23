include config

CATEGORIES		:= hash block mode mac stream prng bignum pkenc pksig

TEST_SRC		+= libmd/testsuite.c
TEST_OBJ		:= ${SRC:.c=.o} ${TEST_SRC:.c=.o}
TEST_EXE		:= libmd/testsuite

PLUG_SRC		+= test/plugin-main.c
PLUG_OBJ		:= ${SRC:.c=.o} ${PLUG_SRC:.c=.o}
PLUG_EXE		:= test/plugin-main

RM				?= rm
RMDIR			?= rmdir

INSTALL_PROG	?= install
INSTALL_OPTS	:= $(shell [ `id -u` -eq 0 ] && printf -- "-o root -g root\n" || printf "\n")
INSTALL			:= $(INSTALL_PROG) $(INSTALL_OPTS)

ifdef PROF
CLIKEFLAGS		+= -pg
endif
ifeq ($(CFG_THREAD_SAFE),y)
CLIKEFLAGS		+= -D_REENTRANT -D_THREAD_SAFE
endif
ifeq ($(CFG_FORTIFY),y)
CLIKEFLAGS		+= -D_FORTIFY_SOURCE=2
endif
ifeq ($(CFG_STACK_CHECK),y)
CLIKEFLAGS		+= -fstack-protector
endif

CPPFLAGS		+= -Iinclude
CLIKEFLAGS		+= -Wall -fPIC -O3 -g -pipe -D_POSIX_SOURCE=200112L -D_XOPEN_SOURCE=600
CLIKEFLAGS		+= -floop-interchange -floop-block
CLIKEFLAGS		+= ${CFLAGS-y}
CXXFLAGS		:= ${CLIKEFLAGS}
CFLAGS			:= ${CLIKEFLAGS}
CXXFLAGS		+= -fno-rtti -fno-exceptions
CFLAGS			+= -std=c99

LIBCFLAGS		+= -shared
PLUGINCFLAGS	+= -I.

LDFLAGS			+= -Wl,--version-script,misc/limited-symbols.ld -Wl,--as-needed
LIBS			+= ${LDFLAGS} -lrt -ldl

.TARGET			= $@
.ALLSRC			= $^
.IMPSRC			= $<

all:

include lib/libdrew/Makefile
include $(patsubst %,impl/%/Makefile,$(CATEGORIES))
include lib/libdrew-impl/Makefile
include test/Makefile
include util/Makefile
include libmd/Makefile

all: ${PLUG_EXE} ${DREW_SONAME} standard

standard: ${DREW_SONAME} ${MD_SONAME} plugins libmd/testsuite
standard: $(TEST_BINARIES) $(UTILITIES)

${TEST_EXE}: ${TEST_SRC} ${MD_SONAME} ${DREW_SONAME} ${DREW_IMPL_SONAME}
	${CC} -Ilibmd/include ${CFLAGS} -o ${.TARGET} ${.ALLSRC} ${LIBS}

${PLUG_EXE}: ${PLUG_OBJ} ${DREW_SONAME}
	${CC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC} ${LIBS}

.c.o:
	${CC} ${CPPFLAGS} ${CFLAGS} -c -o ${.TARGET} ${.IMPSRC}

.cc.o:
	${CXX} ${CPPFLAGS} ${CXXFLAGS} -c -o ${.TARGET} ${.IMPSRC}

${PLUGINS:=.o}: CPPFLAGS += ${PLUGINCFLAGS}

${PLUGINS}: %: %.so
	@:

$(PLUGINS:=.so): %.so: %.o
	${CXX} ${LIBCFLAGS} ${CXXFLAGS} -o ${.TARGET} ${.ALLSRC} ${LIBS}

plugins: ${PLUGINS}
	[ -d plugins ] || mkdir plugins
	for i in ${.ALLSRC}; do cp $$i.so plugins/`basename $$i .so`; done

clean:
	${RM} -f *.o test/*.o
	${RM} -f ${TEST_EXE}
	${RM} -f ${PLUG_EXE}
	${RM} -f ${MD_SONAME} ${MD_OBJS}
	${RM} -f ${DREW_SONAME} ${DREW_SYMLINK}
	${RM} -f ${TEST_BINARIES}
	${RM} -f ${UTILITIES}
	${RM} -fr ${PLUGINS} plugins/
	${RM} -r install
	find -name '*.o' | xargs -r rm
	find -name '*.so' | xargs -r rm
	find -name '*.so.*' | xargs -r rm

test: .PHONY

test check: test-scripts testx-scripts test-libmd
speed speed-test: speed-scripts

test-libmd: ${TEST_EXE}
	env LD_LIBRARY_PATH=. ./${TEST_EXE} -x | \
		grep -v 'bytes in' | diff -u libmd/test-results -

test-scripts: $(TEST_BINARIES) plugins
	set -e; for i in $(CATEGORIES); do \
		find plugins -type f | sed -e 's,.*/,,g' | \
		sort | grep -vE '.rdf$$' | \
		xargs env LD_LIBRARY_PATH=. test/test-$$i -i; \
		done

testx-scripts: $(TEST_BINARIES) plugins
	set -e; for i in $(CATEGORIES); do \
		find plugins -type f | sed -e 's,.*/,,g' | \
		sort | grep -vE '.rdf$$' | \
		xargs env LD_LIBRARY_PATH=. test/test-$$i -t; \
		done

speed-scripts: $(TEST_BINARIES) plugins
	for i in $(CATEGORIES); do \
		find plugins -type f | sed -e 's,.*/,,g' | \
		sort | grep -vE '.rdf$$' | \
		xargs env LD_LIBRARY_PATH=. test/test-$$i -s; \
		done

install: .PHONY

INSTDIR			:= $(CFG_INSTALL_DIR)

install: all
	$(INSTALL) -m 755 -d $(INSTDIR)/lib/drew/plugins
	for i in plugins/*; do $(INSTALL) -m 644 $$i $(INSTDIR)/lib/drew/plugins; done
	$(INSTALL) -m 644 libdrew*.so.* $(INSTDIR)/lib

uninstall:
	$(RM) $(INSTDIR)/lib/libdrew*.so.*
	for i in plugins/*; do $(RM) $(INSTDIR)/lib/drew/$$i; done
	$(RMDIR) $(INSTDIR)/lib/drew/plugins || true
	$(RMDIR) $(INSTDIR)/lib/drew || true
