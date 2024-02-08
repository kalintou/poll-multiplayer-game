CC=g++
CFLAGS=-c #-Wall
LDFLAGS=
SOURCES=rps_client.cpp rps_server.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLES=rps_client rps_server

all: $(EXECUTABLES)

$(EXECUTABLES): $(OBJECTS)
	$(CC) $(LDFLAGS) $@.o -o $@

%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(EXECUTABLES)
