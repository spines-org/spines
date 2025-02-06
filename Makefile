CC = gcc

OBJECTS = util/alarm.o util/events.o util/memory.o util/data_link.o \
	  node.o link.o network.o reliable_link.o link_state.o \
	  protocol.o hello.o route.o udp.o reliable_udp.o session.o \
	  reliable_session.o spines.o

CFLAGS = -g -Wall -O3 -I stdutil/src
STDLIB = stdutil/lib/libstdutil.a

all: stdutil spines spines_lib.a sp_r sp_s sp_flooder sp_ping sping t_flooder


stdutil: stdutil/src/Makefile
	cd stdutil/src; make

stdutil/src/Makefile:
	cd stdutil/src; ./configure



spines: $(OBJECTS)
	$(CC) -o spines $(OBJECTS) $(STDLIB)

spines_lib.a: util/alarm.o util/events.o util/memory.o util/data_link.o spines_lib.o
	ar r spines_lib.a spines_lib.o util/alarm.o util/events.o util/memory.o util/data_link.o; ranlib spines_lib.a

util/alarm.o: util/alarm.c
util/events.o: util/events.c
util/memory.o: util/memory.c
util/data_link.o: util/data_link.c

node.o: node.c
link.o: link.c
network.o: network.c
reliable_link.o: reliable_link.c
link_state.o: link_state.c
hello.o: hello.c
protocol.o: protocol.c
route.o: route.c
udp.o: udp.c
rel_udp.o: rel_udp.c
session.o: session.c
reliable_session.o: reliable_session.c
spines.o: spines.c


spines_lib.o: spines_lib.c

sp_s: sp_s.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a
	$(CC) -o sp_s sp_s.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a 

sp_r: sp_r.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a
	$(CC) -o sp_r sp_r.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a

sp_ping: sp_ping.o spines_lib.a
	$(CC) -o sp_ping sp_ping.o spines_lib.a 

sp_flooder: sp_flooder.o spines_lib.a
	$(CC) -o sp_flooder sp_flooder.o spines_lib.a 

sping: sping.o
	$(CC) -o sping sping.o 

t_flooder: t_flooder.o
	$(CC) -o t_flooder t_flooder.o 


clean:
	rm -f *~
	rm -f *.o
	rm -f util/*.o
	rm -f util/*~
	rm -f core
	cd stdutil/src; make clean




