CC = gcc
# usaremos gcc para invocar a 'ld'
LD = gcc

NAME = doomb

SOURCE_DIR = ./
HEADER_DIR = ./
OUTPUT_DIR = out

SOURCES_C := $(shell find ./ -name '*.c')
HEADERS_C := $(shell find ./ -name '*.h')

OBJECTS_C := $(addprefix $(OUTPUT_DIR)/,$(SOURCES_C:./%.c=%.o))

LIBS = -lm -lcsfml-graphics -lcsfml-window -lcsfml-system

OPT_LEVEL = 3

CFLAGS = -Wall -std=c99 -pedantic
LDFLAGS =

# Compilar main
$(NAME): $(OBJECTS_C)
	$(LD) -o $(NAME) $(LIBS) $^ $(LDFLAGS)

# Compilar cada .c
$(OBJECTS_C): $(OUTPUT_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADERS_C)
	$(CC) -c $(CFLAGS) -O$(OPT_LEVEL) $< -o $@ -I $(HEADER_DIR)

# Borrar objetos
clean:
	rm -f $(shell find -iname *.o)

# Borrar todo
cleanall: clean
	rm -f $(NAME)
