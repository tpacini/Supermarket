AR          =  ar
ARFLAGS     =  rvs

CC		    =  gcc
CFLAGS	    = -std=c99 -Wall -g 
INCLUDES	= -I .
LDFLAGS 	= -L ./lib
OPTFLAGS 	= -O3 -DNDEBUG 
LDLIBS      = -lpthread 

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
# '@' before the command, to not print command
supermarket: src/supermarket.o src/glob.o src/cashier.o
	$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

director: src/director.o src/glob.o
	$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

src/supermarket.o: src/supermarket.c src/supermarket.h

src/director.o: src/director.c src/director.h

src/customer.o: src/customer.c src/customer.h

src/cashier.o : src/cashier.c src/cashier.h 
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ -llibBQueue

src/glob.o : src/glob.c src/glob.h

lib/libBQueue.a: lib/boundedqueue.h  
	$(AR) $(ARFLAGS) $@ $^


clean:
	@rm -f *.txt src/*.o lib/*.o lib/*.a
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

