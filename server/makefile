TARGET  := aesdsocket
SOURCES := aesdsocket.c
OBJECTS := $(SOURCES:.c=.o)

CC := $(CROSS_COMPILE)gcc
CFLAGS += -Wall -Werror
CFLAGS_DEBUG += $(CFLAGS) -g -O0

Debug: $(SOURCES)
	@echo "--- Debug ---"
	@echo "Compiler: " $(CC)
	@echo "--- start compile sources" $< " ---"
	$(CC) $(CFLAGS_DEBUG) -c $^ -o $(TARGET)
	@echo "--- end compile sources" $< " ---"

all: $(TARGET)
	@echo "Compiler: " $(CC)

# generic rule to make objects files from C source files
.c.o:
	@echo "--- start compile sources" $< " ---"
	$(CC) $(CFLAGS) -c $^ -o $@
	@echo "--- end compile sources" $< " ---"

$(TARGET): $(OBJECTS)
	@echo "--- start build target" $@ " ---"
	$(CC) $(OBJECTS) -o $@
	@echo "--- end build target" $@ " ---"

.PHONY: clean
cleanDebug: clean
clean:
	rm -f $(TARGET) $(OBJECTS)
