CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude -std=c11

COMMON_SRC = src/lexer.c src/parser.c src/assembler.c src/object.c

ASM_OUT    = assembler
LNK_OUT    = linker
DMP_OUT    = objdump

all: $(ASM_OUT) $(LNK_OUT) $(DMP_OUT)

$(ASM_OUT): $(COMMON_SRC) src/main.c
	$(CC) $(CFLAGS) $^ -o $@

$(LNK_OUT): $(COMMON_SRC) src/linker.c
	$(CC) $(CFLAGS) $^ -o $@

$(DMP_OUT): $(COMMON_SRC) src/objdump.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
ifeq ($(OS),Windows_NT)
	if exist $(ASM_OUT) del $(ASM_OUT)
	if exist $(ASM_OUT).exe del $(ASM_OUT).exe
	if exist $(LNK_OUT) del $(LNK_OUT)
	if exist $(LNK_OUT).exe del $(LNK_OUT).exe
	if exist $(DMP_OUT) del $(DMP_OUT)
	if exist $(DMP_OUT).exe del $(DMP_OUT).exe
else
	rm -f $(ASM_OUT) $(LNK_OUT) $(DMP_OUT)
endif

.PHONY: all clean