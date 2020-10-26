/*********************************************
 * Aidan Smith, aipsmith@ucsc.edu            *
 * CSE 130, Spring Quarter, April 16th, 2020 *
 * Assignment 0: dog.c                       *
 *********************************************/

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define bufferSize 32768 // Buffer size is defined as 32 KiB.
const int noInput = 1;   // When only the executable is called (no files or symbols follow).
const int endOfFile = 0; // When the end of the file is reached when reading.
const int error = -1;    // When there is an error in open, read, or write.

static int openFile(char* argv){ // Function for opening files.
	int fileNum;
	if(argv[0] == '-'){
		fileNum = STDIN_FILENO;
	}
	else{
		fileNum = open(argv, O_RDONLY);
	}
	if(fileNum == error){
		warn("%s",argv);
	}
	return fileNum;
}

static void writeFile(int fileNum, char* argv){ // Function for reading and writing from the file(s) or terminal.
	void* buffer = malloc(bufferSize);
	int writeByte;	
	int readByte;
	while((readByte = read(fileNum, buffer, sizeof(buffer))) != endOfFile){
		if(readByte == error){
			warn("%s",argv);
			return;
		}
		writeByte = write(STDOUT_FILENO, buffer, readByte);
		if(writeByte == error){
			warn("%s", argv);
			return;
		}
	}
	free(buffer);
}

int main(int argc, char* argv[]){ // Main function for reading in command line arguments.
	if(argc == noInput){
		writeFile(STDIN_FILENO, argv[0]);
	}
	int fileNum;
	for(int i = argc - 1; i > 0; i--){
		fileNum = openFile(argv[i]);
		if(fileNum != error){
			writeFile(fileNum, argv[i]);
			close(fileNum);
		}
	}
	return EXIT_SUCCESS;
}
