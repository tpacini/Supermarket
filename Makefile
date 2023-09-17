AR          =  ar
ARFLAGS     =  rvs

CC		    =  gcc
CFLAGS	    = -std=c99 -Wall -g 
INCLUDES	= -I.
LDFLAGS 	= -L.
OPTFLAGS 	= -O3 -DNDEBUG 
LIBS        = -lpthread 

TARGETS		= supermarket director

# if there is a file called test, this will not create any issue when 
# calling "make test"
.PHONY: clean test1 test2 all 


%: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)


# target : prerequisites, following recipe
supermarket: src/glob.o
	@$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

director: src/supermarket.o src/glob.o
	@$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

libBQueue.a:  lib/boundedqueue.o  lib/boundedqueue.h  
	@$(AR) $(ARFLAGS) $@ $^


clean:
	@rm -f *.txt src/*.a src/*.o lib/*.o lib/*.a
	@rm -f director supermarket

test1:
	@./director 2 20 5 500 80 30 & 
	@sleep 15
	@killall -s SIGHUP director
	@echo "Done!"

test2:
	@chmod +x analisi.sh
	@./director 6 50 3 200 100 20 & 
	@sleep 25
	@killall -s SIGHUP director

# @./analisi.sh

