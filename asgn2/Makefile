#/***********************************************
# * Aidan Smith, aipsmith@ucsc.edu              *
# * CSE 130, Spring Quarter, May 19th, 2020     *
# * Assignment 2: Makefile                      *
# ***********************************************/

#------------------------------------------------------------------------------
#       make                 makes httpserver
#       make clean           cleans all but executable
#       make spotless        cleans everything including executable
#------------------------------------------------------------------------------

httpserver : httpserver.o 
	gcc -o httpserver -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread httpserver.o

httpserver.o : httpserver.c
	gcc -c -Wall -Wextra -Wpedantic -Wshadow -O2 -pthread httpserver.c

clean :
	rm -f httpserver.o

spotless :
	make clean
	rm -f httpserver
