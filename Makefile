CROSS_COMPILE?=
ASAN ?= false

CC = $(CROSS_COMPILE)gcc

GIT_VERSION ?= $(shell git describe --tags --abbrev=4 --dirty --always)

CFLAGS = -MMD -MP -O3 -g -march=native
CFLAGS += -DVERSION="\"$(GIT_VERSION)\""
CFLAGS += -std=gnu11
CFLAGS += -Wall -Wextra
CFLAGS += -Ilibldac/inc -Ilibldac/src
#CFLAGS += -DDEBUG
CFLAGS += -DDEBUG_ADAPTATION
LDLIBS = -lm

ifeq ($(ASAN),true)
LCFLAGS += -fsanitize=address
LDFLAGS += -fsanitize=address
endif

VPATH += libldac/src/
LDFLAGS += -L.

all: libldacdec.so ldacdec ldacenc

libldacdec.so: LDFLAGS += -shared -fpic -Wl,-soname,libldacdec.so.1
libldacdec.so: CFLAGS += -fpic
libldacdec.so: libldacdec.o bit_allocation.o huffCodes.o bit_reader.o utility.o imdct.o spectrum.o

ldacenc: ldacenc.o ldaclib.o ldacBT.o

ldacenc: LDLIBS += $(shell pkg-config sndfile --libs) $(shell pkg-config samplerate --libs)
ldacenc: ldacenc.o ldaclib.o ldacBT.o

ldacdec: libldacdec.so ldacdec.o
ldacdec: LDFLAGS += -Wl,-rpath=.
ldacdec: LDLIBS += -lldacdec -lsndfile

mdct_imdct: LDLIBS += $(shell pkg-config sndfile --libs)
#mdct_imdct: CFLAGS += -DSINGLE_PRECISION
mdct_imdct: mdct_imdct.o ldaclib.o imdct.o

install: libldacdec.so
	ln -sf libldacdec.so libldacdec.so.1
	cp -a libldacdec.so libldacdec.so.1 /usr/lib/
	cp libldacBT_dec.h /usr/include/ldac/libldacBT_dec.h
%.so:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.d *.o ldacenc ldacdec libldacdec.so libldacdec.so.1

-include *.d


