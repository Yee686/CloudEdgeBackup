# Makefile for rsync. This is processed by configure to produce the final
# Makefile

prefix=/usr/local
datarootdir=${prefix}/share
exec_prefix=${prefix}
stunnel4=stunnel
bindir=${exec_prefix}/bin
mandir=${datarootdir}/man

LIBS=
CC=gcc
CFLAGS=-I./zlib -I./popt -g -O2 -DHAVE_CONFIG_H -Wall -W
CPPFLAGS=
EXEEXT=
LDFLAGS=
LIBOBJDIR=lib/

INSTALLCMD=/usr/bin/install -c
INSTALLMAN=/usr/bin/install -c

srcdir=.
MKDIR_P=/usr/bin/mkdir -p

SHELL=/bin/sh

VERSION=3.1.3

.SUFFIXES:
.SUFFIXES: .c .o

GENFILES=configure.sh aclocal.m4 config.h.in proto.h proto.h-tstamp rsync.1 rsyncd.conf.5
HEADERS=byteorder.h config.h errcode.h proto.h rsync.h ifuncs.h itypes.h inums.h \
	lib/pool_alloc.h
LIBOBJ=lib/wildmatch.o lib/compat.o lib/snprintf.o lib/mdfour.o lib/md5.o \
	lib/permstring.o lib/pool_alloc.o lib/sysacls.o lib/sysxattrs.o 
zlib_OBJS=zlib/deflate.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o \
	zlib/trees.o zlib/zutil.o zlib/adler32.o zlib/compress.o zlib/crc32.o
OBJS1=flist.o rsync.o generator.o receiver.o cleanup.o sender.o exclude.o \
	util.o util2.o main.o checksum.o match.o syscall.o log.o backup.o delete.o
OBJS2=options.o io.o compat.o hlink.o token.o uidlist.o socket.o hashtable.o \
	fileio.o batch.o clientname.o chmod.o acls.o xattrs.o
OBJS3=progress.o pipe.o
DAEMON_OBJ = params.o loadparm.o clientserver.o access.o connection.o authenticate.o
popt_OBJS=popt/findme.o  popt/popt.o  popt/poptconfig.o \
	popt/popthelp.o popt/poptparse.o
OBJS=$(OBJS1) $(OBJS2) $(OBJS3) $(DAEMON_OBJ) $(LIBOBJ) $(zlib_OBJS) $(popt_OBJS)

TLS_OBJ = tls.o syscall.o lib/compat.o lib/snprintf.o lib/permstring.o lib/sysxattrs.o $(popt_OBJS)

# Programs we must have to run the test cases
CHECK_PROGS = rsync$(EXEEXT) tls$(EXEEXT) getgroups$(EXEEXT) getfsdev$(EXEEXT) \
	testrun$(EXEEXT) trimslash$(EXEEXT) t_unsafe$(EXEEXT) wildtest$(EXEEXT)

CHECK_SYMLINKS = testsuite/chown-fake.test testsuite/devices-fake.test testsuite/xattrs-hlink.test

# Objects for CHECK_PROGS to clean
CHECK_OBJS=tls.o testrun.o getgroups.o getfsdev.o t_stub.o t_unsafe.o trimslash.o wildtest.o

# note that the -I. is needed to handle config.h when using VPATH
.c.o:
#
	$(CC) -I. -I$(srcdir) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
#

all: Makefile rsync$(EXEEXT) rsync-ssl stunnel-rsync stunnel-rsyncd.conf man-copy

install: all
	-${MKDIR_P} ${DESTDIR}${bindir}
	${INSTALLCMD} ${INSTALL_STRIP} -m 755 rsync$(EXEEXT) ${DESTDIR}${bindir}
	-${MKDIR_P} ${DESTDIR}${mandir}/man1
	-${MKDIR_P} ${DESTDIR}${mandir}/man5
	if test -f rsync.1; then ${INSTALLMAN} -m 644 rsync.1 ${DESTDIR}${mandir}/man1; fi
	if test -f rsyncd.conf.5; then ${INSTALLMAN} -m 644 rsyncd.conf.5 ${DESTDIR}${mandir}/man5; fi

install-ssl-client: rsync-ssl stunnel-rsync
	-${MKDIR_P} ${DESTDIR}${bindir}
	${INSTALLCMD} -m 755 rsync-ssl ${DESTDIR}${bindir}
	${INSTALLCMD} -m 755 stunnel-rsync ${DESTDIR}${bindir}

install-ssl-daemon: stunnel-rsyncd.conf
	-${MKDIR_P} ${DESTDIR}/etc/stunnel
	${INSTALLCMD} -m 644 stunnel-rsyncd.conf ${DESTDIR}/etc/stunnel/rsyncd.conf
	@if ! ls /etc/rsync-ssl/certs/server.* >/dev/null 2>/dev/null; then \
	    echo "Note that you'll need to install the certificate used by /etc/stunnel/rsyncd.conf"; \
	fi

install-all: install install-ssl-client install-ssl-daemon

install-strip:
	$(MAKE) INSTALL_STRIP='-s' install

rsync$(EXEEXT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(OBJS): $(HEADERS)
$(CHECK_OBJS): $(HEADERS)

flist.o: rounding.h

rounding.h: rounding.c rsync.h proto.h
	@for r in 0 1 3; do \
	    if $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o rounding -DEXTRA_ROUNDING=$$r -I. $(srcdir)/rounding.c >rounding.out 2>&1; then \
		echo "#define EXTRA_ROUNDING $$r" >rounding.h; \
		if test -f "$$HOME/build_farm/build_test.fns"; then \
		    echo "EXTRA_ROUNDING is $$r" >&2; \
		fi; \
		break; \
	    fi; \
	done
	@rm -f rounding
	@if test -f rounding.h; then : ; else \
	    cat rounding.out 1>&2; \
	    echo "Failed to create rounding.h!" 1>&2; \
	    exit 1; \
	fi
	@rm -f rounding.out

tls$(EXEEXT): $(TLS_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TLS_OBJ) $(LIBS)

testrun$(EXEEXT): testrun.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ testrun.o

getgroups$(EXEEXT): getgroups.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ getgroups.o $(LIBS)

getfsdev$(EXEEXT): getfsdev.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ getfsdev.o $(LIBS)

TRIMSLASH_OBJ = trimslash.o syscall.o lib/compat.o lib/snprintf.o
trimslash$(EXEEXT): $(TRIMSLASH_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(TRIMSLASH_OBJ) $(LIBS)

T_UNSAFE_OBJ = t_unsafe.o syscall.o util.o util2.o t_stub.o lib/compat.o lib/snprintf.o lib/wildmatch.o
t_unsafe$(EXEEXT): $(T_UNSAFE_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(T_UNSAFE_OBJ) $(LIBS)

gen: conf proto.h man

gensend: gen
	rsync -aivzc $(GENFILES) $${SAMBA_HOST-samba.org}:/home/ftp/pub/rsync/generated-files/

conf:
	cd $(srcdir) && $(MAKE) -f prepare-source.mak conf

aclocal.m4: $(srcdir)/m4/*.m4
	aclocal -I $(srcdir)/m4

configure.sh config.h.in: configure.ac aclocal.m4
	@if test -f configure.sh; then cp -p configure.sh configure.sh.old; else touch configure.sh.old; fi
	@if test -f config.h.in; then cp -p config.h.in config.h.in.old; else touch config.h.in.old; fi
	autoconf -o configure.sh
	autoheader && touch config.h.in
	@if diff configure.sh configure.sh.old >/dev/null 2>&1; then \
	    echo "configure.sh is unchanged."; \
	    rm configure.sh.old; \
	else \
	    echo "configure.sh has CHANGED."; \
	fi
	@if diff config.h.in config.h.in.old >/dev/null 2>&1; then \
	    echo "config.h.in is unchanged."; \
	    rm config.h.in.old; \
	else \
	    echo "config.h.in has CHANGED."; \
	fi
	@if test -f configure.sh.old -o -f config.h.in.old; then \
	    if test "$(MAKECMDGOALS)" = reconfigure; then \
		echo 'Continuing with "make reconfigure".'; \
	    else \
		echo 'You may need to run:'; \
		echo '  make reconfigure'; \
		exit 1; \
	    fi \
	fi

reconfigure: configure.sh
	./config.status --recheck
	./config.status

Makefile: Makefile.in config.status configure.sh config.h.in
	@if test -f Makefile; then cp -p Makefile Makefile.old; else touch Makefile.old; fi
	@./config.status
	@if diff Makefile Makefile.old >/dev/null 2>&1; then \
	    echo "Makefile is unchanged."; \
	    rm Makefile.old; \
	else \
	    if test "$(MAKECMDGOALS)" = reconfigure; then \
		echo 'Continuing with "make reconfigure".'; \
	    else \
		echo "Makefile updated -- rerun your make command."; \
		exit 1; \
	    fi \
	fi

rsync-ssl: $(srcdir)/rsync-ssl.in Makefile
	sed 's;\@bindir\@;$(bindir);g' <$(srcdir)/rsync-ssl.in >rsync-ssl
	@chmod +x rsync-ssl

stunnel-rsync: $(srcdir)/stunnel-rsync.in Makefile
	sed 's;\@stunnel4\@;$(stunnel4);g' <$(srcdir)/stunnel-rsync.in >stunnel-rsync
	@chmod +x stunnel-rsync

stunnel-rsyncd.conf: $(srcdir)/stunnel-rsyncd.conf.in Makefile
	sed 's;\@bindir\@;$(bindir);g' <$(srcdir)/stunnel-rsyncd.conf.in >stunnel-rsyncd.conf

proto: proto.h-tstamp

proto.h: proto.h-tstamp
	@if test -f proto.h; then :; else cp -p $(srcdir)/proto.h .; fi

proto.h-tstamp: $(srcdir)/*.c $(srcdir)/lib/compat.c config.h
	perl $(srcdir)/mkproto.pl $(srcdir)/*.c $(srcdir)/lib/compat.c

man: rsync.1 rsyncd.conf.5 man-copy

man-copy:
	@if test -f rsync.1; then :; elif test -f $(srcdir)/rsync.1; then echo 'Copying srcdir rsync.1'; cp -p $(srcdir)/rsync.1 .; else echo "NOTE: rsync.1 cannot be created."; fi
	@if test -f rsyncd.conf.5; then :; elif test -f $(srcdir)/rsyncd.conf.5; then echo 'Copying srcdir rsyncd.conf.5'; cp -p $(srcdir)/rsyncd.conf.5 .; else echo "NOTE: rsyncd.conf.5 cannot be created."; fi

rsync.1: rsync.yo
	yodl2man -o rsync.1 $(srcdir)/rsync.yo
	-$(srcdir)/tweak_manpage rsync.1

rsyncd.conf.5: rsyncd.conf.yo
	yodl2man -o rsyncd.conf.5 $(srcdir)/rsyncd.conf.yo
	-$(srcdir)/tweak_manpage rsyncd.conf.5

clean: cleantests
	rm -f *~ $(OBJS) $(CHECK_PROGS) $(CHECK_OBJS) $(CHECK_SYMLINKS) \
		rounding rounding.h *.old

cleantests:
	rm -rf ./testtmp*

# We try to delete built files from both the source and build
# directories, just in case somebody previously configured things in
# the source directory.
distclean: clean
	rm -f Makefile config.h config.status
	rm -f rsync-ssl stunnel-rsync stunnel-rsyncd.conf
	rm -f lib/dummy popt/dummy zlib/dummy
	rm -f $(srcdir)/Makefile $(srcdir)/config.h $(srcdir)/config.status
	rm -f $(srcdir)/lib/dummy $(srcdir)/popt/dummy $(srcdir)/zlib/dummy
	rm -f config.cache config.log
	rm -f $(srcdir)/config.cache $(srcdir)/config.log
	rm -f shconfig $(srcdir)/shconfig
	rm -f $(GENFILES)
	rm -rf autom4te.cache

# this target is really just for my use. It only works on a limited
# range of machines and is used to produce a list of potentially
# dead (ie. unused) functions in the code. (tridge)
finddead:
	nm *.o */*.o |grep 'U ' | awk '{print $$2}' | sort -u > nmused.txt
	nm *.o */*.o |grep 'T ' | awk '{print $$3}' | sort -u > nmfns.txt
	comm -13 nmused.txt nmfns.txt

# 'check' is the GNU name, 'test' is the name for everybody else :-)
.PHONY: check test

test: check

# There seems to be no standard way to specify some variables as
# exported from a Makefile apart from listing them like this.

# This depends on building rsync; if we need any helper programs it
# should depend on them too.

# We try to run the scripts with POSIX mode on, in the hope that will
# catch Bash-isms earlier even if we're running on GNU.  Of course, we
# might lose in the future where POSIX diverges from old sh.

check: all $(CHECK_PROGS) $(CHECK_SYMLINKS)
	rsync_bin=`pwd`/rsync$(EXEEXT) $(srcdir)/runtests.sh

check29: all $(CHECK_PROGS) $(CHECK_SYMLINKS)
	rsync_bin=`pwd`/rsync$(EXEEXT) $(srcdir)/runtests.sh --protocol=29

check30: all $(CHECK_PROGS) $(CHECK_SYMLINKS)
	rsync_bin=`pwd`/rsync$(EXEEXT) $(srcdir)/runtests.sh --protocol=30

wildtest.o: wildtest.c lib/wildmatch.c rsync.h config.h
wildtest$(EXEEXT): wildtest.o lib/compat.o lib/snprintf.o $(popt_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ wildtest.o lib/compat.o lib/snprintf.o $(popt_OBJS) $(LIBS)

testsuite/chown-fake.test:
	ln -s chown.test $(srcdir)/testsuite/chown-fake.test

testsuite/devices-fake.test:
	ln -s devices.test $(srcdir)/testsuite/devices-fake.test

testsuite/xattrs-hlink.test:
	ln -s xattrs.test $(srcdir)/testsuite/xattrs-hlink.test

# This does *not* depend on building or installing: you can use it to
# check a version installed from a binary or some other source tree,
# if you want.

installcheck: $(CHECK_PROGS) $(CHECK_SYMLINKS)
	POSIXLY_CORRECT=1 TOOLDIR=`pwd` rsync_bin="$(bindir)/rsync$(EXEEXT)" srcdir="$(srcdir)" $(srcdir)/runtests.sh

# TODO: Add 'dist' target; need to know which files will be included

# Run the SPLINT (Secure Programming Lint) tool.  <www.splint.org>
.PHONY: splint
splint:
	splint +unixlib +gnuextensions -weak rsync.c

doxygen:
	cd $(srcdir) && rm dox/html/* && doxygen

# for maintainers only
doxygen-upload:
	rsync -avzv $(srcdir)/dox/html/ --delete \
	$${SAMBA_HOST-samba.org}:/home/httpd/html/rsync/doxygen/head/
