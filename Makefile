CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lX11

TARGET = myterm
SOURCES = main.c x11_gui.c shell_process.c text_buffer.c tab_manager.c external_commands.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install-deps:
	sudo apt-get update
	sudo apt-get install -y libx11-dev

.PHONY: all clean install-deps
