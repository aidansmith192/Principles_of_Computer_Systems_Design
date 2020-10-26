/***********************************************
 * Aidan Smith, aipsmith@ucsc.edu              *
 * CSE 130, Spring Quarter, May 4th, 2020      *
 * Assignment 1: server.c                      *
 ***********************************************/

#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h> // true, false
#include <errno.h>
#include <sys/types.h> //fstat
#include <inttypes.h> // uintptr_t
#include <ctype.h> // isalnum

#define BUFFER_SIZE 4096
#define FILE_END 0
#define ERROR -1
#define OK 200
#define CREATED 201
#define CLIENT_ERROR 400
extern int errno;

struct httpObject {
	/*
	   Create some object 'struct' to keep track of all
	   the components related to a HTTP message
NOTE: There may be more member variables you would want to add
*/
	char method[5];                     // PUT, HEAD, GET
	char filename[28];                  // what is the file we are worried about
	char httpversion[9];                // HTTP/1.1
	ssize_t request_content_length;     // content length of the request from client. (example: 13)
	ssize_t response_content_length;    // content length of the response from server.
	int status_code;                    // ERROR, OK, CREATED, or CLIENT_ERROR
	uint8_t buffer[BUFFER_SIZE];        // for holding currently read bytes
	uint8_t* contentBuffer;             // for holding the content when going from request to process
	ssize_t contentBufferBytes;         // for holding length of content between functions request and process
};

/*
   \brief 3. Construct some response based on the HTTP request you recieved
   */
void construct_http_response(ssize_t client_sockd, struct httpObject* message) {
	printf("Constructing Response\n");
	if(message->status_code == ERROR){
		if(errno == 2){ // 404 not found
			dprintf(client_sockd, "%s 404 Not Found\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}else if(errno == 13){ // 403 forbidden
			dprintf(client_sockd, "%s 403 Forbidden\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}else{ // 500 server error
			dprintf(client_sockd, "%s 500 Internal Server Error\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
		}
	}else if(message->status_code == CLIENT_ERROR){ // 400 client error
		dprintf(client_sockd, "%s 400 Bad Request\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else if(message->status_code == OK){ // 200 Ok
		printf("TESSSTTTTTERINO\n");
		dprintf(client_sockd, "%s 200 OK\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else if(message->status_code == CREATED){ // 201 Created
		dprintf(client_sockd, "%s 201 Created\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}else{
		dprintf(client_sockd, "%s 500 Internal Server Error\r\nContent-Length: %ld\r\n\r\n", message->httpversion, message->response_content_length);
	}
	return;
}

/*
   \brief 2. Want to process the message we just recieved
   */
void process_request(ssize_t client_sockd, struct httpObject* message) {
	printf("Processing Request\n"); 
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
		message->status_code = OK;
		construct_http_response(client_sockd, message); // 200
	}else if(strcmp(message->method, "PUT") == 0){ // 201 if create file. content length zero
		const char* contentString = "Content-Length:";
		char* contentHolder;
		contentHolder = strstr((char*)message->buffer, contentString);
		message->request_content_length = atoi(contentHolder + 15);
		int fileNum;
		fileNum = open(message->filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
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

/*
   \brief 1. Want to read in the HTTP message/ data coming in from socket
   \param client_sockd - socket file descriptor
   \param message - object we want to 'fill in' as we read in the HTTP message
   */
void read_http_request(ssize_t client_sockd, struct httpObject* message) {
	printf("This function will take care of reading message\n");
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
		char slashFilename[29];
		sscanf((char*)message->buffer, "%s %s %s", message->method, slashFilename, message->httpversion); // read header
		strncpy(message->filename, (slashFilename + 1), 28); // remove slash
		int i = 0;
		while(message->filename[i] != '\0'){
			if((isalnum(message->filename[i]) == 0) && (message->filename[i] != '-') && (message->filename[i] != '_')){
				message->status_code = CLIENT_ERROR;
				construct_http_response(client_sockd, message); //error
				return;
			}
			i++;
		}
		if(strcmp(message->filename, "") == 0){
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

int main(int argc, char* argv[]) {
	/*
	   Create sockaddr_in with server information
	   */
	char* port = argv[1];
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

	struct httpObject message;

	while (true) {
		printf("[+] server is waiting...\n");
		
		/*
		 * 1. Accept Connection
		 */
		int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
		if(client_sockd == ERROR){
			continue; // close client without response, we have no proper fd to send to
		}

		/*
		 * 2. Read HTTP Message
		 * 	2a. Calls process_request if no errors occur
		 * 	2b. Calls construct_http_response before process_request prints anything
		 */
		read_http_request(client_sockd, &message);

		/*
		 * 3. Send Response
		 */
		printf("Response Sent\n");

		/*
		 * Sample Example which wrote to STDOUT once.
		 *
		 uint8_t buff[BUFFER_SIZE + 1];
		 ssize_t bytes = recv(client_sockd, buff, BUFFER_SIZE, 0);
		 buff[bytes] = 0; // null terminate
		 printf("[+] received %ld bytes from client\n[+] response: \n", bytes);
		 write(STDOUT_FILENO, buff, bytes);
		 */

		close(client_sockd);
		memset(&message, 0, sizeof(message));
	}

	return EXIT_SUCCESS;
}
