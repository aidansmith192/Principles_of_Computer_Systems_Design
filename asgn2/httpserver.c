/***********************************************
 * Aidan Smith, aipsmith@ucsc.edu              *
 * CSE 130, Spring Quarter, May 19th, 2020     *
 * Assignment 2: httpserver.c                  *
 ***********************************************/

#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h>    // write
#include <string.h>    // memset
#include <stdlib.h>    // atoi
#include <stdbool.h>   // true, false
#include <errno.h>     // error numbers
#include <sys/types.h> // fstat
#include <inttypes.h>  // uintptr_t
#include <ctype.h>     // isalnum
#include <pthread.h>   // threads
#include <sys/wait.h>  // wait
#include <signal.h>    // signal

#define BUFFER_SIZE 4096
#define LOG_BUFFER_SIZE 69
#define CONTENT_SIZE 20
#define FOOTER_SIZE 9
#define FILE_END 0
#define ERROR -1
#define OK 200
#define CREATED 201
#define CLIENT_ERROR 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define SERVER_ERROR 500

extern int errno;

struct httpObject {
	/* Create some object 'struct' to keep track of all
	 * the components related to a HTTP message */
	char method[50];                    // PUT, HEAD, GET
	char filename[100];                 // what is the file we are worried about
	char httpversion[20];               // HTTP/1.1
	ssize_t request_content_length;     // content length of the request from client. (example: 13)
	ssize_t response_content_length;    // content length of the response from server.
	int status_code;                    // ERROR, OK, CREATED, or CLIENT_ERROR
	uint8_t buffer[BUFFER_SIZE];        // for holding currently read bytes
	uint8_t* contentBuffer;             // for holding the content when going from request to process
	ssize_t contentBufferBytes;         // for holding length of content between functions request and process
};

struct threadStruct{
	/* threadStruct has all information relating to the threads contained in here.
	 * It is passed by reference so all threads access the same information. */
	uint16_t queueStart;         // current index that is at the front of the queue
	uint16_t queueEnd;           // current index that is at the end of the queue
	uint16_t numThreads;         // number of threads
	uint32_t numFails;           // number of fails
	uint32_t numRequests;        // number of requests sent to the server
	off_t globalOffset;          // offset between threads for pwrite
	ssize_t logFD;               // file for logging
	uint8_t* queue;              // for containing all the socket IDs
	pthread_cond_t empty;        // conditional for empty
	pthread_cond_t full;         // conditional for full
	pthread_mutex_t queueMutex;  // mutex var for queue
	pthread_mutex_t offsetMutex; // mutex var for offset
};

// the healthcheck version of construct_http_response
void construct_health_check(ssize_t client_sockd, struct httpObject* message, struct threadStruct* threadStruct){
	char contentLength[20];
	sprintf(contentLength, "%d %d", threadStruct->numFails, threadStruct->numRequests);
	message->response_content_length = strlen(contentLength);
	message->status_code = OK;
	dprintf(client_sockd, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n%d\n%d", message->httpversion, message->response_content_length, threadStruct->numFails, threadStruct->numRequests);
	return;
}

/* brief 3. Construct some response based on the HTTP request you recieved */
void construct_http_response(ssize_t client_sockd, struct httpObject* message) {
	if(message->status_code == ERROR){
		if(errno == 2){ // 404 not found
			message->status_code = NOT_FOUND;
			dprintf(client_sockd, "%s 404 Not Found\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}else if(errno == 13){ // 403 forbidden
			message->status_code = FORBIDDEN;
			dprintf(client_sockd, "%s 403 Forbidden\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}else{ // 500 server error
			message->status_code = SERVER_ERROR;
			dprintf(client_sockd, "%s 500 Internal Server Error\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}
	}else if(message->status_code == CLIENT_ERROR){ // 400 client error
		dprintf(client_sockd, "%s 400 Bad Request\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else if(message->status_code == OK){ // 200 Ok
		dprintf(client_sockd, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else if(message->status_code == CREATED){ // 201 Created
		dprintf(client_sockd, "%s 201 Created\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else{ // unsure, its a server error
		message->status_code = SERVER_ERROR;
		dprintf(client_sockd, "%s 500 Internal Server Error\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}
	return;
}

/* brief 2. Want to process the message we just recieved */
void process_request(ssize_t client_sockd, struct httpObject* message) {
	if(strcmp(message->method, "GET") == 0){ // content length non-zero
		ssize_t fileNum;
		fileNum = open(message->filename, O_RDONLY);
		if(fileNum == ERROR){
			message->status_code = ERROR;
			construct_http_response(client_sockd, message); // 404 not found or 500 server error
		}else{
			ssize_t writeByte;
			ssize_t readByte;
			if((readByte = read(fileNum, message->buffer, BUFFER_SIZE)) != FILE_END){ // read once and print status message before writing
				if(readByte == ERROR){
					message->status_code = ERROR;
					construct_http_response(client_sockd, message); // 500
					close(fileNum); // close before exiting
					return; // error
				}
				struct stat statbuf;
				fstat(fileNum, &statbuf); // to read the length of the file
				if(statbuf.st_size == ERROR){ // if fstat fails
					message->status_code = ERROR;
					construct_http_response(client_sockd, message);
					close(fileNum); // close before exiting
					return; // error
				}
				message->response_content_length = statbuf.st_size;
				message->status_code = OK;
				construct_http_response(client_sockd, message); // 200
				writeByte = write(client_sockd, message->buffer, readByte);
				if(writeByte == ERROR){
					close(fileNum); // close before exiting
					return; // error
				}
			}
			while((readByte = read(fileNum, message->buffer, BUFFER_SIZE)) != FILE_END){ // continue reading and writing and exit if error
				if(readByte == ERROR){
					break; // error, break so that it can close
				}
				writeByte = write(client_sockd, message->buffer, readByte);
				if(writeByte == ERROR){
					break; // error, break so that it can close
				}
			}
			close(fileNum);
		}
	}else if(strcmp(message->method, "HEAD") == 0){ // content length zero
		ssize_t fileNum;
		fileNum = open(message->filename, O_RDONLY);
		if(fileNum == ERROR){
			message->status_code = ERROR;
			construct_http_response(client_sockd, message); // 404 not found or 500 server error
			return; // error
		}
		struct stat statbuf;
		fstat(fileNum, &statbuf); // to read the length of the file
		close(fileNum);
		if(statbuf.st_size == ERROR){ // if fstat fails
			message->status_code = ERROR;
			construct_http_response(client_sockd, message);
			return; // error
		}
		message->response_content_length = statbuf.st_size;
		message->status_code = OK;
		construct_http_response(client_sockd, message); // 200
	}else if(strcmp(message->method, "PUT") == 0){ // 201 if create file. content length zero
		const char* contentString = "Content-Length:";
		char* contentHolder;
		contentHolder = strstr((char*)message->buffer, contentString);
		message->request_content_length = atoi(contentHolder + 15);
		int fileNum;
		fileNum = open(message->filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if(fileNum == ERROR){ // error
			message->status_code = ERROR;
			construct_http_response(client_sockd, message); // error
		}else{
			ssize_t writeByte;
			if(message->contentBufferBytes > 0){ // if the content from previous function isnt empty, then print that first
				writeByte = write(fileNum, message->contentBuffer, message->contentBufferBytes);
				if(writeByte == ERROR){
					close(fileNum);
					return; // error
				}
			}
			ssize_t readByte = 1;
			ssize_t totalReadBytes = message->contentBufferBytes;
			ssize_t differenceBytes = 0;
			if(message->request_content_length - message->contentBufferBytes < BUFFER_SIZE){ // read smaller buffer
				differenceBytes = BUFFER_SIZE - message->request_content_length;
			}
			while((totalReadBytes < message->request_content_length) && (readByte != 0)){ // need to make sure to read the exact content
				readByte = read(client_sockd, message->buffer, BUFFER_SIZE - differenceBytes);
				if(readByte == ERROR){
					break; // error, break so that it can close
				}
				writeByte = write(fileNum, message->buffer, readByte);
				if(writeByte == ERROR){
					break; // error, break so that it can close
				}
				totalReadBytes += readByte;
				if((message->request_content_length - totalReadBytes) < BUFFER_SIZE){ // read smaller buffer
					differenceBytes = BUFFER_SIZE - (message->request_content_length - totalReadBytes);
				}	
			}
			message->status_code = CREATED;
			construct_http_response(client_sockd, message); // 201 file created
			close(fileNum);
		}
	}else{ // bad request 400
		message->status_code = CLIENT_ERROR;
		construct_http_response(client_sockd, message);
	}
	return;
}

/* brief 1. Want to read in the HTTP message/ data coming in from socket
   \param client_sockd - socket file descriptor
   \param message - object we want to 'fill in' as we read in the HTTP message */
void read_http_request(ssize_t client_sockd, struct httpObject* message, struct threadStruct* threadStruct) {
	ssize_t bytesRead;
	if((bytesRead = recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) != FILE_END){ // read from file
		if(bytesRead == ERROR){
			message->status_code = ERROR;
			construct_http_response(client_sockd, message); // error
			return; // exit
		}
		const char* headerEnd = "\r\n\r\n"; // find where header ends
		message->contentBuffer = (uint8_t*) strstr((char*)message->buffer, headerEnd);
		ssize_t totalBytesRead;
		totalBytesRead = bytesRead;
		while(message->contentBuffer == NULL){ // we have not hit the double new line yet!
			if((bytesRead = recv(client_sockd, (message->buffer + totalBytesRead), BUFFER_SIZE - totalBytesRead, 0)) != FILE_END){
				totalBytesRead += bytesRead;
				if(bytesRead == ERROR){
					message->status_code = ERROR;
					construct_http_response(client_sockd, message); // error
					return;
				}
				message->contentBuffer = (uint8_t*) strstr((char*)message->buffer, headerEnd); // keep checking whole buffer
			}
		} // once loop exits, we either hit double new line or are over the limit of 4096 bytes for header
		if(message->contentBuffer == NULL){ // double new line still not hit? error. header cannot be more than 4096 bytes
			message->status_code = CLIENT_ERROR;
			construct_http_response(client_sockd, message); // header too large, 400 client error
			return;
		}
		char slashFilename[101];
		sscanf((char*)message->buffer, "%s %s %s", message->method, slashFilename, message->httpversion); // read header
		strncpy(message->filename, (slashFilename + 1), 100); // remove slash
		int i = 0;
		while(message->filename[i] != '\0'){
			if(i > 27){
				message->status_code = CLIENT_ERROR;
				construct_http_response(client_sockd, message); //error
				return;
			}
			if((isalnum(message->filename[i]) == 0) && (message->filename[i] != '-') && (message->filename[i] != '_')){
				message->status_code = CLIENT_ERROR;
				construct_http_response(client_sockd, message); //error
				return;
			}
			i++;
		}
		if(strcmp(message->filename, "healthcheck") == 0){ // healthcheck
			if(strcmp(message->method, "GET") != 0){ // if not get, 403
				message->status_code = ERROR;
				errno = 13;
				construct_http_response(client_sockd, message); //error, must be get if healthcheck
				return;
			}else if(threadStruct->logFD == 0){ // if there is no log, 404
				message->status_code = ERROR;
				errno = 2;
				construct_http_response(client_sockd, message); //error, must be a log file
				return;
			}
			construct_health_check(client_sockd, message, threadStruct);
			return;
		}else if(strcmp(message->filename, "") == 0){
			message->status_code = CLIENT_ERROR;
			construct_http_response(client_sockd, message); //error
			return;
		}
		if(strcmp(message->httpversion, "HTTP/1.1") != 0){
			message->status_code = CLIENT_ERROR;
			construct_http_response(client_sockd, message); //error
			return;
		}
		message->contentBuffer = (message->contentBuffer + 4); // skip past new lines... they count as header
		message->contentBufferBytes = bytesRead - (ssize_t)((uintptr_t)message->contentBuffer - (uintptr_t)message->buffer); // length of content
		process_request(client_sockd, message);
	}else{
		message->status_code = ERROR;
		construct_http_response(client_sockd, message); // no bytes receieved, 400 client error
	}
	return;
}

void write_to_log(struct httpObject* message, struct threadStruct* threadStruct){
	off_t localOffset; // offset for this thread relative to its own previous lines
	off_t bodyOffset;  // offset from body of log
	ssize_t bytesWritten;
	ssize_t totalBytesWritten = 0;
	uint8_t logBuffer[LOG_BUFFER_SIZE * 5]; // for reading the "any" type, made large to include the incorrect entries made
	//char logHexBuffer[LOG_BUFFER_SIZE + 1]; // converted ascii into hex. size is + 1 for null terminator
	size_t headerLength;
	memset(message->buffer, 0, sizeof(message->buffer));

	// ============================== HEADER =============================================== //
	if((message->status_code == OK) || (message->status_code == CREATED)){ // if there was no error
		if(message->status_code == OK)
			sprintf((char*)message->buffer, "%s /%s length %ld\n", message->method, message->filename, message->response_content_length);
		else
			sprintf((char*)message->buffer, "%s /%s length %ld\n", message->method, message->filename, message->request_content_length);
	}else{                         		// if there was an error
		sprintf((char*)message->buffer, "FAIL: %s /%s %s --- response %d\n", message->method, message->filename, message->httpversion, message->status_code);
	
	}
	headerLength = strlen((char*)message->buffer);
	message->contentBufferBytes = headerLength;

	// calculating offset created by body
	if(strcmp(message->method, "HEAD") == 0){ // if HEAD, HEAD doesnt have a body
		bodyOffset = 0;
	}else if(message->status_code == OK){
		if(strcmp(message->filename, "healthcheck") == 0){ // if its a healthcheck
			bodyOffset = (message->response_content_length * 3) + 8 + 1; // total bytes to write
		}else{ // not a healthcheck
			bodyOffset = (message->response_content_length) / CONTENT_SIZE; // number of lines created by the body
			bodyOffset = bodyOffset * LOG_BUFFER_SIZE; // all the full lines, total bytes
			uint8_t overflow = message->response_content_length % CONTENT_SIZE;
			if(overflow != 0){
				overflow = 1;
			}
			bodyOffset += ((message->response_content_length % CONTENT_SIZE) * 3) + (9 * overflow); // the last line
		}
	}else if(message->status_code == CREATED){ // CREATED (PUT)
		bodyOffset = (message->request_content_length) / CONTENT_SIZE; // number of lines created by the body
		bodyOffset = bodyOffset * LOG_BUFFER_SIZE; // all the full lines, total bytes
		uint8_t overflow = message->request_content_length % CONTENT_SIZE;
		if(overflow != 0){
			overflow = 1;
		}
		bodyOffset += ((message->request_content_length % CONTENT_SIZE) * 3) + (9 * overflow); // the last line added, gives us offset from body!
	}else{ // error
		bodyOffset = 0;
	}

	pthread_mutex_lock(&threadStruct->offsetMutex); // lock mutex
	
	localOffset = threadStruct->globalOffset; // local offset for this thread
	threadStruct->globalOffset += headerLength + bodyOffset + FOOTER_SIZE; // length from header, length from body, 9 from footer. offset for next thread
	
	pthread_mutex_unlock(&threadStruct->offsetMutex); // unlock mutex
	
	bodyOffset += localOffset + headerLength; // used later in body
	
	// ============================== BODY =============================================== //
        if((message->status_code == OK) || (message->status_code == CREATED)){ // if there was no error, errors dont have a body
		if(strcmp(message->filename, "healthcheck") == 0){ // special print for health check
			char contentLength[20];
			
			sprintf((char*)(message->buffer + message->contentBufferBytes), "00000000");
			message->contentBufferBytes += 8;
			
			sprintf(contentLength, "%d\n%d", threadStruct->numFails, threadStruct->numRequests);
			uint8_t* contentBuffer = (uint8_t*)contentLength;
			for(int i = 0; i < message->response_content_length; i++){ // goes until it hits the total # of bytes read
				sprintf((char*)(message->buffer + message->contentBufferBytes), " %02x", contentBuffer[i]);
				message->contentBufferBytes += 3;
			}
			sprintf((char*)(message->buffer + message->contentBufferBytes), "\n");
			message->contentBufferBytes++;
		}else if(strcmp(message->method, "HEAD") != 0){ // if not HEAD, HEAD doesnt have a body
			uint32_t lineNumber = 0; // must be at least 24 bits to contain an 8 digit number
			ssize_t bytesRead;
			ssize_t totalBytesRead = 0;
			
			ssize_t fileNum;
			fileNum = open(message->filename, O_RDONLY);
			if(fileNum == ERROR){
				//error
			}
			while(localOffset + message->contentBufferBytes < bodyOffset){ // keeps going through all lines of content
				while((bytesRead = read(fileNum, (logBuffer + totalBytesRead), CONTENT_SIZE - totalBytesRead)) != FILE_END){
					if(bytesRead == ERROR){ // reads 20 per line until it hits the last line
						// error
					}
					totalBytesRead += bytesRead;
				}
				sprintf((char*)(message->buffer + message->contentBufferBytes), "%08d", lineNumber);
				message->contentBufferBytes += 8;
				lineNumber += CONTENT_SIZE; // increase the line counter after saving the previous
				for(int i = 0;i < totalBytesRead; i++){ // goes until it hits the total # of bytes read
					sprintf((char*)(message->buffer + message->contentBufferBytes), " %02x", logBuffer[i]);
					message->contentBufferBytes += 3;
				}
				sprintf((char*)(message->buffer + message->contentBufferBytes), "\n");
				message->contentBufferBytes++;
				totalBytesRead = 0;
				if(message->contentBufferBytes > BUFFER_SIZE - 100){
					while((bytesWritten = pwrite(threadStruct->logFD, (message->buffer + totalBytesWritten), message->contentBufferBytes - totalBytesWritten, localOffset)) != FILE_END){
						if(bytesWritten == ERROR){
							//error
						}
						localOffset += (off_t)bytesWritten;
						totalBytesWritten += bytesWritten;
					}
					totalBytesWritten = 0;
					message->contentBufferBytes = 0;
				}
			}
			close(fileNum);
		}
        }
	
	// ============================== FOOTER =============================================== //
	strcpy((char*)(message->buffer + message->contentBufferBytes), "========\n");
	message->contentBufferBytes += 9;
	bytesWritten = 0;
	totalBytesWritten = 0;
	while((bytesWritten = pwrite(threadStruct->logFD, (message->buffer + totalBytesWritten), message->contentBufferBytes - totalBytesWritten, localOffset)) != FILE_END){
		if(bytesWritten == ERROR){
			// error
		}
		localOffset += (off_t)bytesWritten;
		totalBytesWritten += bytesWritten;
	}

	if((message->status_code != OK) && (message->status_code != CREATED)){ // if there was an error
		threadStruct->numFails++; // add to the number of fails
	}
	threadStruct->numRequests++; // add to the number of requests counter. do it here so we dont add healthchecks
	return;
}

void* threadFunction(void* voidStruct){ // function for thread
	struct threadStruct* threadStruct = (struct threadStruct*) voidStruct;
	struct httpObject message;
	memset(&message, 0, sizeof(message));
	int client_sockd;
	while(true){
		pthread_mutex_lock(&threadStruct->queueMutex);
		while(threadStruct->queueStart == threadStruct->queueEnd){
			pthread_cond_wait(&threadStruct->empty, &threadStruct->queueMutex);
		}

		client_sockd = threadStruct->queue[threadStruct->queueStart]; // dequeue
		if(threadStruct->queueStart == threadStruct->numThreads*2){
			threadStruct->queueStart = 0;
		}else{
			threadStruct->queueStart++; // move front of queue over by one
		}

		pthread_mutex_unlock(&threadStruct->queueMutex); // unlock mutex for another threads to use
		pthread_cond_signal(&threadStruct->full); // signal that it should no longer be full
		
		/*
		 * 2. Read HTTP Message
		 * 	2a. Calls process_request if no errors occur
		 * 	2b. Calls construct_http_response before process_request prints anything
		 */
		read_http_request(client_sockd, &message, threadStruct);

		/*
		 * 3. Send Response
		 */
		if(threadStruct->logFD != 0) // if a log was given
			write_to_log(&message, threadStruct);

		close(client_sockd);
		memset(&message, 0, sizeof(message));
	}
}

int main(int argc, char* argv[]) {
	if((argc != 2) && (argc != 4) && (argc != 6)){ // only possible argc values without error
		printf("Invalid use, refer to README.md.\n");
		return(EXIT_FAILURE);
	}
	char* charThreads = "4"; // default number of threads is 4
	uint8_t numThreads;
	ssize_t fd = 0;
	char* port;
	int flag;
	while((flag = getopt(argc, argv, "l:N:")) != ERROR){
		switch(flag){
			case 'l':
				if((fd = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == ERROR){ // save log fd
					//error
				}
				break;
			case 'N':
				charThreads = optarg; // number of threads in char form
				break;
			default:
				break;
		}
	}
	if(optind == ERROR){
		// error
	}
	port = argv[optind]; // save port

	numThreads = atoi(charThreads); // convert char* to int for number of threads
	pthread_t threads[numThreads]; // perhaps instead keep track of a number XXX

	struct threadStruct threadStruct;
	memset(&threadStruct, 0, sizeof(threadStruct));
	threadStruct.numThreads = numThreads;
	threadStruct.logFD = fd;

	uint8_t queueSize = numThreads*2;
	uint8_t queueTemp[queueSize];
	threadStruct.queue = queueTemp;

	if(pthread_mutex_init(&threadStruct.queueMutex, NULL) != 0){
		// error, not clients fault. try running it again?...
	}
	if(pthread_mutex_init(&threadStruct.offsetMutex, NULL) != 0){
		// error, not clients fault. try running it again?...
	}
	if(pthread_cond_init(&threadStruct.empty, NULL) != 0){
		//error, not clients fault
	}
	if(pthread_cond_init(&threadStruct.full, NULL) != 0){
		//error, not clients fault
	}

	for(int i = 0; i < numThreads; i++){ // creating N threads, defaults to 4 without input
		if(pthread_create(&threads[i], NULL, threadFunction, (void*)&threadStruct) != 0){
			// error
		}
	}

	/*
	   Create sockaddr_in with server information
	   */
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port));
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	socklen_t addrlen = sizeof(server_addr);

	/*
	   Create server socket
	   */
	int server_sockd = socket(AF_INET, SOCK_STREAM, 0);

	// Need to check if server_sockd < 0, meaning an error
	if (server_sockd < 0) {
		perror("socket");
	}

	/*
	   Configure server socket
	   */
	int enable = 1;

	/*
	   This allows you to avoid: 'Bind: Address Already in Use' error
	   */
	int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	/*
	   Bind server address to socket that is open
	   */
	ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

	/*
	   Listen for incoming connections
	   */
	ret = listen(server_sockd, 5); // 5 should be enough, if not use SOMAXCONN

	if (ret < 0) {
		return EXIT_FAILURE;
	}

	/*
	   Connecting with a client
	   */
	struct sockaddr client_addr;
	socklen_t client_addrlen;
		
	while (true) {	
		/*
		 * 1. Accept Connection
		 */
		int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
		if(client_sockd == ERROR){
			continue; // close client without response, we have no proper fd to send to
		}
		pthread_mutex_lock(&threadStruct.queueMutex);
		// if queue is full
		while(threadStruct.queueEnd == (threadStruct.queueStart - 1) || ((threadStruct.queueStart == 0) && (threadStruct.queueEnd == (numThreads*2)))){
			pthread_cond_wait(&threadStruct.full, &threadStruct.queueMutex);
		}
		threadStruct.queue[threadStruct.queueEnd] = client_sockd; // add to queue
		
		if(threadStruct.queueEnd != queueSize){
			threadStruct.queueEnd++;
		}else{
			threadStruct.queueEnd = 0;
		}
		pthread_mutex_unlock(&threadStruct.queueMutex);
		pthread_cond_signal(&threadStruct.empty); // signal to a thread that there is a queued client
	}
	return EXIT_SUCCESS;
}
