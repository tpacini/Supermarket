AR          =  ar
ARFLAGS     =  rvs

CC		    =  gcc
CFLAGS	    = -std=c99 -Wall -g 
INCLUDES	= -I.
LDFLAGS 	= -L.
OPTFLAGS 	= -O3 -DNDEBUG 
LIBS        = -lpthread 

TARGETS		= director 
.PHONY: cleanall test 

%: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

supermercato: supermercato.o cassa.o cliente.o libBQueue.a 
	@$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

libBQueue.a:  lib/boundedqueue.o  lib/boundedqueue.h  
	@$(AR) $(ARFLAGS) $@ $^



test:
	@chmod +x analisi.sh
	@./$(TARGETS) 6 50 3 & 
	@sleep 25
	@killall -s SIGHUP supermercato
	@./analisi.sh

cleanall:
	@rm -f *.txt *.a *.o lib/*.o lib/*.a
	@rm -f director 
