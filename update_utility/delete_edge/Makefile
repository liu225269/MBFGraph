# ssdsim linux support
CPPS := $(wildcard *.cpp)
OBJS = $(patsubst %.cpp,%.o,$(CPPS))
LDFLAGS=-L/usr/lib/x86_64-linux-gnu/
CFLAGS= -Wall -std=c++11 -g -O3 
#CFLAGS= -Wall -O3
STACFLAGS= -static -std=c++11 -Wall -O3

#LDFLAGS=-I/home/mdl/cql5513/my_boost/include/ -L/home/mdl/cql5513/my_boost/lib/ -lboost_program_options
#CFLAGS= -Wall -g -I/home/mdl/cql5513/my_boost/include/

CC=g++
#CC=g++-9
all:delete_page
	
clean:
	rm -f delete_page *.o *~
.PHONY: clean

delete_page: ${OBJS}
	$(CC) $(CFLAGS) -o delete_page $(OBJS) $(LDFLAGS)
	#$(CC) $(STACFLAGS) -o delete_page_s $(OBJS) $(LDFLAGS)
%.o: %.cpp  
	$(CC) $(CFLAGS) -c $<  -o $@

