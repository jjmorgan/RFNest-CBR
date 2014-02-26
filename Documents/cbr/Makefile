all: cbr

cbr: cbr.o
	g++ -Wall -o cbr cbr.o

cbr.o: cbr.cpp
	g++ -c cbr.cpp

clean:
	rm -rf *.o cbr
