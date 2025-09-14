NAME := $(shell basename $(PWD))
export MODULE := M5

SRCS   := $(shell find tests/ -maxdepth 1 -name "*.c")
CFLAGS := -I.
CFLAGS += -I$(HOME)/local/include
CFLAGS += -I../testkit

all: $(NAME) check

check:
	gcc -DFREESTANDING -I. \
	    -ffreestanding -static -nostdlib -nostartfiles -no-pie \
	    -o malloc-check *.c && \
	        rm -f malloc-check

test-verbose: $(NAME)
	TK_VERBOSE=1 TK_RUN=1 ./$(NAME)

include ../.shadow/oslabs.mk
