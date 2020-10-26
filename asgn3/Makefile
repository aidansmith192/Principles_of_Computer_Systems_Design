#/***********************************************
# * Aidan Smith, aipsmith@ucsc.edu              *
# * CSE 130, Spring Quarter, June 8th, 2020     *
# * Assignment 3: Makefile                      *
# ***********************************************/

#------------------------------------------------------------------------------
#       make                 makes httpserver
#       make clean           cleans all but executable
#       make spotless        cleans everything including executable
#------------------------------------------------------------------------------

loadbalancer : loadbalancer.o 
	gcc -o loadbalancer -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread loadbalancer.o

loadbalancer.o : loadbalancer.c
	gcc -c -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread loadbalancer.c

clean :
	rm -f loadbalancer.o

spotless :
	make clean
	rm -f loadbalancer
