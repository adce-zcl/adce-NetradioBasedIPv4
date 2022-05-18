CFLAGS+=-I../include/ -g
LDFLAGS+=-pthread

all:server
server:server.o thr_channel.o thr_list.o mytbf.o medialib.o
	$(CC) $^ -o $@ $(CFLAGS) #(LDFLAGS)

clean:
	rm -rf *.o server