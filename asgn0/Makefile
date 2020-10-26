#/***********************************************
# * Aidan Smith, aipsmith@ucsc.edu              *
# * CSE 130, Spring Quarter, April 16th, 2020   *
# * Assignment 0: Makefile                      *
# ***********************************************/

#------------------------------------------------------------------------------
#       make                 makes dog
#       make clean           cleans all but executable
#       make spotless        cleans everything including executable
#------------------------------------------------------------------------------

dog : dog.o 
	gcc -o dog -Wall -Wextra -Wpedantic -Wshadow -O2 dog.o

dog.o : dog.c
	gcc -c -Wall -Wextra -Wpedantic -Wshadow -O2 dog.c

clean :
	rm -f dog.o

spotless :
	make clean
	rm -f dog
