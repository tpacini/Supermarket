AR          =  ar
ARFLAGS     =  rvs

CC		    =  gcc
CFLAGS	    = -std=c99 -Wall -g 
INCLUDES	= -I .
LDFLAGS 	= -L./lib
OPTFLAGS 	= -O3 -DNDEBUG 
LDLIBS      = -lpthread -lboundedqueue

TARGETS		= lib/libboundedqueue.so director

# if there is a file called test1, this will not create any issue when 
# calling "make test1"
.PHONY: clean test1 test2 all 

%.o: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

all: $(TARGETS)

# target : prerequisites, following recipe
# '@' before the command, to not print command
lib/libboundedqueue.so: lib/boundedqueue.c
	$(CC) -c -Wall -Werror -fpic $< -o lib/boundedqueue.o
	$(CC) -shared -o $@ lib/boundedqueue.o

director: src/director.o src/glob.o src/supermarket.o src/cashier.o src/customer.o
	$(CC) $(CCFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	@rm -f src/*.o lib/*.o lib/*.so
	@rm -f director
test1:
	@./director 2 20 5 500 80 30 & 
	@sleep 15
	@killall -s SIGHUP director
	@tail --pid=$$(pidof director) -f /dev/null
	@echo "Done!"

test2:
	@./director 6 50 3 200 100 20 & 
	@sleep 25
	@killall -s SIGQUIT director
	@echo "Done!"

test3:
	@./director 2 20 5 500 80 30 & 
	@sleep 25
	@killall -s SIGQUIT director
	@echo "Done!"
	@chmod +x analisi.sh
	@./analisi.sh


#export LD_LIBRARY_PATH=/home/ranxerox/Documenti/Supermarket/lib:$LD_LIBRARY_PATH
	