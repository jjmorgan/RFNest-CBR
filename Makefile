all: cbr cbrfile

cbr: cbr.o
	g++ -Wall -o cbr cbr.o

cbrfile: cbrfile.o
	g++ -Wall -o cbrfile cbrfile.o

cbr.o: cbr.cpp
	g++ -c cbr.cpp

cbrfile.o: cbrfile.cpp
	g++ -c cbrfile.cpp

clean:
	rm -rf *.o cbr cbrfile
