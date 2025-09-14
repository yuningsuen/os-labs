NAME := $(shell basename $(PWD))
export MODULE := M4
CFLAGS += -I$(HOME)/local/include
LDFLAGS += -ldl -L$(HOME)/local/lib -lreadline -lncurses

all: $(NAME)

test: $(NAME)
	TK_RUN=1 ./$(NAME)

test-verbose: $(NAME)
	TK_VERBOSE=1 TK_RUN=1 ./$(NAME)

include ../.shadow/oslabs.mk
