
include ../Make.conf


default: all

all: pre $(BUILD)/libold.so $(BUILD)/oldtest $(BUILD)/oldtest2

install: all
	install -g root -o root -m 0755 $(BUILD)/libold.so $(PREFIX)/lib
	install -g root -o root -m 0644 old.h $(PREFIX)/include
	@echo "Please run 'ldconfig' to update your library cache"

pre:
	@if [ ! -d $(BUILD) ]; then mkdir $(BUILD); fi

$(BUILD)/libold.so: libold.o
	$(CC) $(CFLAGS) --shared -o $(BUILD)/libold.so libold.o $(INCLUDES) $(LIBS)

$(BUILD)/oldtest: libold.o oldtest.o
	$(CC) $(CFLAGS) -o $(BUILD)/oldtest libold.o oldtest.o $(INCLUDES) $(LIBS)

$(BUILD)/oldtest2: libold.o oldtest2.o
	$(CC) $(CFLAGS) -o $(BUILD)/oldtest2 libold.o oldtest2.o $(INCLUDES) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean cleanobj
cleanobj:
	rm -f libold.o
	rm -f oldtest.o
	rm -f oldtest2.o

cleandebug:
	rm -f *.bb *.bbg *.da *.gcov gmon.out

clean: cleanobj cleandebug
	rm -f $(BUILD)/oldtest
	rm -f $(BUILD)/oldtest2
	rm -f $(BUILD)/libold.so

