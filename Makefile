
CFLAGS += -p -O2 -Wall
CFLAGS += -D_GNU_SOURCE
CFLAGS += -DWITH_UGID
CFLAGS += -DWITH_LIBBLKID
CFLAGS += -DEVHEAD_MAGIC=1234
LDFLAGS += -lblkid

OBJ_diskmountd = \
		 util.o \
		 nlsock.o \
		 evsock.o \
		 diskev.o \
		 disktab.o \
		 diskconf.o \
		 diskmountd.o

OBJ_diskmount = \
		 util.o \
		 evsock.o \
		 diskev.o \
		 diskmount.o

all: diskmountd diskmount

clean:
	rm -f diskmountd diskmount *.o

%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS-$<) -c -o $@ $<

diskmountd: $(OBJ_diskmountd)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

diskmount: $(OBJ_diskmount)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
