
include Make.conf


# objects to build
OBJS = hash.o list.o lock.o main.o net.o wqueue.o

# rules
default: all

all: pre lib $(BUILD)/old

pre:
	@if [ ! -d $(BUILD) ]; then mkdir $(BUILD); fi

install: all lib_install man_install
	install -g root -o root -m 0755 $(BUILD)/old $(PREFIX)/bin


$(BUILD)/old: ${OBJS}
	$(CC) $(CFLAGS) -o $(BUILD)/old $(OBJS) $(LIBS)
	#strip $(BUILD)/old

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


# the libs have their own Makefile
lib:
	@$(MAKE) -C lib/

lib_install:
	@$(MAKE) -C lib/ install

# manpages
man_install:
	install -g root -o root -m 0755 -d $(PREFIX)/man/man1
	install -g root -o root -m 0644 doc/old.1 $(PREFIX)/man/man1/
	install -g root -o root -m 0755 -d $(PREFIX)/man/man3
	install -g root -o root -m 0644 doc/libold.3 $(PREFIX)/man/man3/

cleanobj:
	rm -f $(OBJS) lib/libold.o

cleandebug:
	rm -f *.bb *.bbg *.da *.gcov gmon.out

clean: cleanobj cleandebug
	rm -rf $(BUILD)
	$(MAKE) -C lib/ clean


.PHONY: clean cleanobj lib

