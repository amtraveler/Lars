TARGET= lars_dns_test
CXX=g++ -std=gnu++11
CFLAGS=-g -O2 -Wall -Wno-deprecated 

BASE=../../base
BASE_H=$(BASE)/include

PROTO = $(BASE)/proto
PROTO_H = $(BASE)/proto

LARS_REACTOR=../../lars_reactor
LARS_REACTOR_H=$(LARS_REACTOR)/include
LARS_REACTOR_LIB=$(LARS_REACTOR)/lib -lreactor

OTHER_LIB= -lpthread -ldl -lprotobuf

INC= -I./include -I$(BASE_H) -I$(LARS_REACTOR_H) -I$(PROTO_H) 

LIB= -L$(LARS_REACTOR_LIB) $(OTHER_LIB) 

OBJS = lars_dns_test.o
OBJS += $(PROTO)/lars.pb.o

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $(TARGET) $(OBJS) $(INC) $(LIB)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

.PHONY: clean
clean:
	-rm -f ./*.o $(TARGET)
