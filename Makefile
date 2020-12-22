#CFLAGS  += -std=c99 -Wall -O2 -D_REENTRANT
CFLAGS  += -Wall

LIBS    := -lm -lpthread

TARGET  := $(shell uname -s | tr '[A-Z]' '[a-z]' 2>/dev/null || echo unknown)

ifeq ($(TARGET), sunos)
	CFLAGS += -D_PTHREADS -D_POSIX_C_SOURCE=200112L
	LIBS   += -lsocket
else ifeq ($(TARGET), darwin)
	LDFLAGS += -pagezero_size 10000 -image_base 100000000
	export MACOSX_DEPLOYMENT_TARGET = $(shell sw_vers -productVersion)
else ifeq ($(TARGET), linux)
	CFLAGS  += -D_POSIX_C_SOURCE=200112L -D_BSD_SOURCE -D_DEFAULT_SOURCE
	LIBS    += -ldl
	LDFLAGS += -Wl,-E
else ifeq ($(TARGET), freebsd)
	CFLAGS  += -D_DECLARE_C99_LDBL_MATH
	LDFLAGS += -Wl,-E
endif

SRC  := wrktcp.c net.c aprintf.c stats.c units.c \
		ae.c zmalloc.c islog.c tcpini.c
BIN  := wrktcp

ODIR := obj
OBJ  := $(patsubst %.c,$(ODIR)/%.o,$(SRC))
LIBS := $(LIBS)

DEPS    :=
CFLAGS  += -I$(ODIR)/include
#LDFLAGS += -L$(ODIR)/lib
LDFLAGS +=

all: $(BIN) test

clean:
	$(RM) -rf $(BIN) obj/* *.log 
	cd ./test && make clean

$(BIN): $(OBJ)
	@echo LINK $(BIN)
	@$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY:test
test: 
	@cd ./test && make clean && make

$(ODIR):
	@mkdir -p $@

$(ODIR)/%.o : %.c
	@echo CC $<
	@$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: all clean

.SUFFIXES:
.SUFFIXES: .c .o

vpath %.c   src
vpath %.h   src
