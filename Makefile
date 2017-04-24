TARGET = bamboozled
LIBS = -pthread -lm
CC = gcc
CFLAGS = -pthread -g -O3 -Wall -std=c99
PREFIX = /usr/local
CONFIG_PATH = /etc/$(TARGET).json

.PHONY: default all clean install uninstall

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp "$(TARGET)" "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"
ifneq ($(wildcard /lib/systemd/system/.),)
	mkdir -p "$(dir $(CONFIG_PATH))"
	cp $(TARGET).json "$(CONFIG_PATH)"
	sed "s/EXECUTABLE_PATH/$(subst /,\/,$(DESTDIR)$(PREFIX)/bin/$(TARGET))/g;s/CONFIG_PATH/$(subst /,\/,$(CONFIG_PATH))/g" $(TARGET).service > /lib/systemd/system/$(TARGET).service
endif

uninstall:
	-rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET)"
ifneq ($(wildcard /lib/systemd/system/.),)
	-rm -f "$(CONFIG_PATH)"
	-systemctl disable $(TARGET).service
	-rm -f /lib/systemd/system/$(TARGET).service
endif
