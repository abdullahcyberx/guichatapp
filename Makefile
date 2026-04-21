CXX      = g++
CXXFLAGS = -Wall -Wextra -g -pthread -std=c++17
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS   := $(shell pkg-config --libs   gtk+-3.0)
TARGETS  = server client

all: $(TARGETS)

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

client: client.cpp
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -o client client.cpp $(GTK_LIBS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
