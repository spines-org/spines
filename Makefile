CC = gcc

OBJECTS = util/alarm.o util/events.o util/memory.o util/data_link.o \
	  node.o link.o network.o reliable_datagram.o state_flood.o \
          link_state.o protocol.o hello.o route.o udp.o reliable_udp.o \
	  session.o reliable_session.o multicast.o \
          spines.o

CFLAGS = -Wall -O3 -I stdutil/src
STDLIB = stdutil/lib/libstdutil.a

all: stdutil spines spines_lib.a test/sp_r test/sp_s test/sp_tflooder test/sp_uflooder test/sp_ping test/sping test/t_flooder test/u_flooder test/setloss


stdutil: stdutil/src/Makefile
	cd stdutil/src; make

stdutil/src/Makefile:
	cd stdutil/src; ./configure



spines: $(OBJECTS)
	$(CC) -o spines $(OBJECTS) $(STDLIB) -lm

spines_lib.a: util/alarm.o util/events.o util/memory.o util/data_link.o spines_lib.o
	ar r spines_lib.a spines_lib.o util/alarm.o util/events.o util/memory.o util/data_link.o; ranlib spines_lib.a

util/alarm.o: util/alarm.c
util/events.o: util/events.c
util/memory.o: util/memory.c
util/data_link.o: util/data_link.c

node.o: node.c
link.o: link.c
network.o: network.c
reliable_datagram.o: reliable_datagram.c
state_flood.o: state_flood.c
link_state.o: link_state.c
hello.o: hello.c
protocol.o: protocol.c
route.o: route.c
udp.o: udp.c
reliable_udp.o: reliable_udp.c
realtime_udp.o: realtime_udp.c
session.o: session.c
multicast.o: multicast.c
reliable_multicast.o: reliable_multicast.c
reliable_session.o: reliable_session.c
spines.o: spines.c


spines_lib.o: spines_lib.c

test/sp_s: test/sp_s.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a
	$(CC) -o test/sp_s test/sp_s.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a 

test/sp_r: test/sp_r.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a
	$(CC) -o test/sp_r test/sp_r.o util/alarm.o util/data_link.o util/events.o util/memory.o spines_lib.a

test/sp_ping: test/sp_ping.o spines_lib.a
	$(CC) -o test/sp_ping test/sp_ping.o spines_lib.a 

test/sp_tflooder: test/sp_tflooder.o spines_lib.a
	$(CC) -o test/sp_tflooder test/sp_tflooder.o spines_lib.a 

test/sp_uflooder: test/sp_uflooder.o spines_lib.a
	$(CC) -o test/sp_uflooder test/sp_uflooder.o spines_lib.a 

test/sping: test/sping.o
	$(CC) -o test/sping test/sping.o 

test/t_flooder: test/t_flooder.o
	$(CC) -o test/t_flooder test/t_flooder.o 

test/u_flooder: test/u_flooder.o
	$(CC) -o test/u_flooder test/u_flooder.o 

test/setloss: test/setloss.o spines_lib.a
	$(CC) -o test/setloss test/setloss.o spines_lib.a 


clean:
	rm -f *~
	rm -f *.o
	rm -f util/*.o
	rm -f test/*.o
	rm -f util/*~
	rm -f test/*~
	rm -f core
	cd stdutil/src; make clean




