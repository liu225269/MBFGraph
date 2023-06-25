# ssdsim linux support
CPPS := $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(CPPS))
LDFLAGS=-L/usr/lib/x86_64-linux-gnu/ -ltbb
CFLAGS= -Wall -std=c++17 -mavx -O3  -fopenmp
#CFLAGS= -Wall -std=c++17 -O3
STACFLAGS= -static -std=c++17 -Wall -O3 -fopenmp

CC=g++
#CC=g++-9
all:MBFSort
	
clean:
	rm -f MBFSort MBFSort_s *.o *~
.PHONY: clean

MBFSort: ${OBJS}
	$(CC) $(CFLAGS) -o MBFSort $(OBJS) $(LDFLAGS)
	#$(CC) $(STACFLAGS) -o MBFSort_s $(OBJS) $(LDFLAGS)
%.o: %.cpp  
	$(CC) $(CFLAGS) -c $<  -o $@

