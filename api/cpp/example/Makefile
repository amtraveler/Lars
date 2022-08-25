CXX=g++
CFLAGS=-g -O2 -Wall -Wno-deprecated


LARS_REACTOR=../../../lars_reactor
LARS_REACTOR_H=$(LARS_REACTOR)/include
LARS_REACTOR_LIB=$(LARS_REACTOR)/lib -lreactor

all:
	$(CXX) $(CFLAGS) -o example example.cpp -I ../lars_api -I$(LARS_REACTOR_H) ../lib/liblarsclient.a -lprotobuf -lpthread
	$(CXX) $(CFLAGS) -o simulator simulator.cpp -I ../lars_api -I$(LARS_REACTOR_H) ../lib/liblarsclient.a -lprotobuf -lpthread
	$(CXX) $(CFLAGS) -o get_route get_route.cpp -I ../lars_api -I$(LARS_REACTOR_H) ../lib/liblarsclient.a -lprotobuf -lpthread
	$(CXX) $(CFLAGS) -o get_host get_host.cpp -I ../lars_api -I$(LARS_REACTOR_H) ../lib/liblarsclient.a -lprotobuf -lpthread
	$(CXX) $(CFLAGS) -o qps qps.cpp -I ../lars_api -I$(LARS_REACTOR_H) ../lib/liblarsclient.a -lprotobuf -lpthread

.PHONY: clean
clean:
	-rm ./example ./simulator ./get_route ./get_host ./qps
