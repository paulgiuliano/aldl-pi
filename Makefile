# compiler flags
CFLAGS= -O2 -Wall
OBJS= acquire.o error.o loadconfig.o useful.o aldlcomm.o aldldata.o consoleif.o remote.o datalogger.o mode4.o
LIBS= -lpthread -lrt -lncurses

# install configuration
CONFIGDIR= /etc/aldl
LOGDIR= /var/log/aldl
BINDIR= /usr/local/bin
BINARIES= aldl-ftdi aldl-tty aldl-dummy

.PHONY: clean install stats

# not building tty driver by default yet
all: aldl-ftdi aldl-tty aldl-dummy
	@echo
	@echo '*********************************************************'
	@echo ' Run the following as root to install the binaries and'
	@echo ' config files:  make install'
	@echo '*********************************************************'
	@echo

# not installing tty driver by default yet
install: aldl-ftdi aldl-dummy
	@echo Installing to $(BINDIR)
	cp -fv $(BINARIES) $(BINDIR)/
	ln -sf $(BINDIR)/aldl-ftdi $(BINDIR)/aldl
	@echo 'Creating directory structure'
	mkdir -pv $(CONFIGDIR)
	mkdir -pv $(LOGDIR)
	@echo 'Copying example configs, will not overwrite...'
	cp -nv ./config-examples/* $(CONFIGDIR)/
	@echo
	@echo '*******************************************************'
	@echo ' No automatic updates of configs are done.  Please see'
	@echo ' examples/ if this was an existing installation, and'
	@echo ' attempt to merge these changes manually...'
	@echo '*******************************************************'
	@echo
	@echo '****************************************************************'
	@echo ' The default log directory is /var/log/aldl, if you are running'
	@echo ' this program as a regular user, you must change permissions on'
	@echo ' that directory...'
	@echo '****************************************************************'
	@echo
	@echo Install complete, see configs in $(CONFIGDIR) before running

aldl-ftdi: main.c serio-ftdi.o config.h aldl-io.h aldl-types.h $(OBJS)
	gcc $(CFLAGS) $(LIBS) -lftdi main.c -o aldl-ftdi $(OBJS) serio-ftdi.o
	@echo
	@echo '***************************************************'
	@echo ' You must blacklist or rmmod the ftdi_sio driver!!'
	@echo ' Debian users can try the debian-config.sh script. '
	@echo '***************************************************'
	@echo

aldl-tty: main.c serio-tty.o config.h aldl-io.h aldl-types.h $(OBJS)
	@echo 'The TTY serial driver is unfinished,'
	@echo 'Using it will simply generate an error.'
	gcc $(CFLAGS) $(LIBS) main.c -o aldl-tty $(OBJS) serio-tty.o

aldl-dummy: main.c serio-dummy.o config.h aldl-io.h aldl-types.h $(OBJS)
	gcc $(CFLAGS) $(LIBS) main.c -o aldl-dummy $(OBJS) serio-dummy.o

useful.o: useful.c useful.h config.h aldl-types.h
	gcc $(CFLAGS) -c useful.c -o useful.o

loadconfig.o: loadconfig.c loadconfig.h config.h aldl-types.h
	gcc $(CFLAGS) -c loadconfig.c -o loadconfig.o

acquire.o: acquire.c acquire.h config.h aldl-io.h aldl-types.h
	gcc $(CFLAGS) -c acquire.c -o acquire.o

error.o: error.c error.h config.h aldl-types.h
	gcc $(CFLAGS) -c error.c -o error.o

serio-ftdi.o: serio-ftdi.c aldl-io.h aldl-types.h config.h
	gcc $(CFLAGS) -c serio-ftdi.c -o serio-ftdi.o

serio-tty.o: serio-tty.c aldl-io.h aldl-types.h config.h
	gcc $(CFLAGS) -c serio-tty.c -o serio-tty.o

serio-dummy.o: serio-dummy.c aldl-io.h aldl-types.h config.h
	gcc $(CFLAGS) -c serio-dummy.c -o serio-dummy.o

aldlcomm.o: aldl-io.h aldlcomm.c aldlcomm.h aldl-types.h serio-ftdi.o config.h
	gcc $(CFLAGS) -c aldlcomm.c -o aldlcomm.o

aldldata.o: aldl-io.h aldl-types.h aldldata.c aldlcomm.o config.h
	gcc $(CFLAGS) -c aldldata.c -o aldldata.o

consoleif.o: consoleif.c modules.h
	gcc -lncurses $(CFLAGS) -c consoleif.c -o consoleif.o

datalogger.o: datalogger.c modules.h
	gcc $(CFLAGS) -c datalogger.c -o datalogger.o

remote.o: remote.c modules.h
	gcc $(CFLAGS) -c remote.c -o remote.o

mode4.o: mode4.c modules.h
	gcc $(CFLAGS) -c mode4.c -o mode4.o

clean:
	rm -fv *.o *.a $(BINARIES)

stats:
	wc -l *.c *.h */*.c */*.h
