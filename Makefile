CC      := clang
PKGS	:= libspotify alsa
CFLAGS  := -g -Wall `pkg-config --cflags $(PKGS)`
LIBS    := `pkg-config --libs $(PKGS)`

TARGET	:= spot
SOURCES := $(shell find src/ -type f -name *.c)
OBJECTS := $(patsubst src/%,build/%,$(SOURCES:.c=.o))

$(TARGET): $(OBJECTS)
	@echo "  Linking..."; $(CC) $^ -o $(TARGET) $(LIBS) -lpthread

build/%.o: src/%.c
	@mkdir -p build/
	@echo "  CC $<"; $(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "  Cleaning..."; $(RM) -r build/ $(TARGET)

.PHONY: clean
