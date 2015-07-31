GEN_SRC = ./gen-cpp/SharedService.cpp ./gen-cpp/shared_types.cpp ./gen-cpp/tutorial_types.cpp ./gen-cpp/Calculator.cpp
CFLAGS += -Wall -ggdb -O2 -DHAVE_INTTYPES_H -DHAVE_NETINET_IN_H -std=c++0x

all: thrift client controller

client: thrift
	g++ ${CFLAGS} -o client  -I./gen-cpp  ${GEN_SRC} client.cpp -lthrift-client -lthrift -lev -lprotobuf -lglog -lgflags

controller: thrift
	g++ ${CFLAGS} -o controller  -I./gen-cpp  ${GEN_SRC} controller.cpp -lthrift-client -lthrift -lev -lprotobuf -lglog -lgflags

thrift:
	thrift --gen cpp shared.thrift
	thrift --gen cpp tutorial.thrift

test:
	./controller --logtostderr=1

clean:
	rm -rf *.o
	rm -rf client controller
