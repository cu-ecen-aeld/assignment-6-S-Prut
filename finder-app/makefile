TARGET  := writer
SOURCES := writer.c
OBJECTS := $(SOURCES:.c=.o)

CC := $(CROSS_COMPILE)gcc
CFLAGS :=

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
clean:
	rm -f $(TARGET) ./*.o
