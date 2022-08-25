TARGET=../lib/liblarsclient.a
CXX=g++ -std=gnu++11
CFLAGS=-g -O2 -Wall -Wno-deprecated 

BASE=../../../base
BASE_H=$(BASE)/include

PROTO = $(BASE)/proto
PROTO_H = $(BASE)/proto

LARS_REACTOR=../../../lars_reactor
LARS_REACTOR_H=$(LARS_REACTOR)/include
LARS_REACTOR_LIB=$(LARS_REACTOR)/lib -lreactor


OTHER_LIB= -lpthread -ldl -lprotobuf
SRC= ./
INC= -I./ -I$(BASE_H) -I$(LARS_REACTOR_H) -I$(PROTO_H) 

LIB= -L$(LARS_REACTOR_LIB) $(OTHER_LIB) 

OBJS = $(addsuffix .o, $(basename $(wildcard $(SRC)/*.cpp)))
OBJS += $(PROTO)/lars.pb.o

$(TARGET): $(OBJS)
	mkdir -p ../bin
	ar cqs $@ $^	

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $< $(INC)

.PHONY: clean
clean:
	-rm -f ./*.o $(TARGET) $(PROTO)/*.o
