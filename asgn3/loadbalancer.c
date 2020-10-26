/***********************************************
 * Aidan Smith, aipsmith@ucsc.edu              *
 * CSE 130, Spring Quarter, June 8th, 2020     *
 * Assignment 3: httpserver.c                  *
 ***********************************************/

// I attended Clark's sessions and my code will reflect that, I did not cheat.

#include <err.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define ERROR -1
#define OK 200
#define BUFFER_SIZE 4096
#define FILE_END 0
#define REQUEST_MAX 65536

struct serverStats{
	uint32_t port; // individual worker port
	bool alive;
	int requests;
	int fails;
};

struct servers{
	uint32_t numServers;
	uint8_t time;
	struct serverStats *serverStats;
	uint32_t port; // load balancer port
	pthread_mutex_t socketMutex;
	pthread_mutex_t healthMutex;
	pthread_cond_t full;
	pthread_cond_t wait;
	uint32_t servicing; // number of threads currently at work
	uint16_t numRequests;
	uint16_t totRequests;
};

struct servers servers;

/*
 * client_connect takes a port number and establishes a connection as a client.
 * connectport: port number of server to connect to
 * returns: valid socket if successful, -1 otherwise
 */
int client_connect(uint16_t connectport) {
	int connfd;
	struct sockaddr_in servaddr;

	connfd=socket(AF_INET,SOCK_STREAM,0);
	if (connfd < 0)
		return -1;
	memset(&servaddr, 0, sizeof servaddr);

	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(connectport);

	/* For this assignment the IP address can be fixed */
	inet_pton(AF_INET,"127.0.0.1",&(servaddr.sin_addr));

	if(connect(connfd,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0)
		return -1;
	return connfd;
}

void *healthCheck(){
	while(true){
		pthread_mutex_lock(&servers.healthMutex);
		struct timespec timeout;
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += servers.time;
		int rc = 0;
		while((servers.numRequests > servers.totRequests) && (rc == 0)){ // signal if time = X or totRequests = R
			rc = pthread_cond_timedwait(&servers.wait, &servers.healthMutex, &timeout);
		}
		servers.totRequests = 0;
		for(uint8_t i = 0; i < servers.numServers; i++){
			int connfd;
			ssize_t writeByte;
			ssize_t writtenBytes = 0;
			uint8_t buffer[BUFFER_SIZE];
			if ((connfd = client_connect(servers.serverStats[i].port)) < 0){
				servers.serverStats[i].alive = false;
				continue;
			}
			char sendChar[29] = "GET /healthcheck HTTP/1.1\r\n\r\n";
			while((writeByte = write(connfd, sendChar + writtenBytes, 29 - writtenBytes)) != 0){
				writtenBytes += writeByte;
				if(writeByte == ERROR){
					servers.serverStats[i].alive = false;
					close(connfd);
					break;
				}
			}
			if(writeByte == ERROR){
				close(connfd);
				break;
			}
			memset(buffer, 0, sizeof(buffer));
			const char* headerEnd = "\r\n\r\n"; // find where header ends
			ssize_t bytesRead;
			ssize_t totalBytesRead = 0;
			while((bytesRead = recv(connfd, buffer + totalBytesRead, BUFFER_SIZE - totalBytesRead, 0)) != FILE_END){ // read from file
				if(bytesRead == ERROR){
					servers.serverStats[i].alive = false;
					close(connfd);
					break;
				}else
					servers.serverStats[i].alive = true;
			}
			close(connfd);
			if(servers.serverStats[i].alive == false)
				continue;
			char charStatus[4];
			int status;
			sscanf((char*)buffer, "HTTP/1.1 %s ", charStatus);
			status = atoi(charStatus);
			
			if(status != OK)
				servers.serverStats[i].alive = false;

			char charFails[6];
			char charRequests[6];
			uint8_t *contentBuffer;
			contentBuffer = (uint8_t*) strstr((char*)buffer, headerEnd);
			sscanf((char*)contentBuffer, "\r\n\r\n%s\n%s",charFails, charRequests);
			servers.serverStats[i].fails = atoi(charFails);
			servers.serverStats[i].requests = atoi(charRequests);
		}
		pthread_mutex_unlock(&servers.healthMutex);
	}
}

/*
 * server_listen takes a port number and creates a socket to listen on 
 * that port.
 * port: the port number to receive connections
 * returns: valid socket if successful, -1 otherwise
 */
int server_listen(int port) {
	int listenfd;
	int enable = 1;
	struct sockaddr_in servaddr;

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0)
		return -1;
	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
		return -1;
	if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof servaddr) < 0)
		return -1;
	if (listen(listenfd, 500) < 0)
		return -1;
	return listenfd;
}

/*
 * bridge_connections send up to 4096 bytes from fromfd to tofd
 * fromfd, tofd: valid sockets
 * returns: number of bytes sent, 0 if connection closed, -1 on error
 */
int bridge_connections(int fromfd, int tofd) {
	char recvline[BUFFER_SIZE];
	int n = recv(fromfd, recvline, BUFFER_SIZE, 0);
	if (n < 0) {
		printf("connection error receiving\n");
		return -1;
	} else if (n == 0) {
		printf("receiving connection ended\n");
		return 0;
	}
	recvline[n] = '\0';
	n = send(tofd, recvline, n, 0);
	if (n < 0) {
		printf("connection error sending\n");
		return -1;
	} else if (n == 0) {
		printf("sending connection ended\n");
		return 0;
	}
	return n;
}

/*
 * bridge_loop forwards all messages between both sockets until the connection
 * is interrupted. It also prints a message if both channels are idle.
 * sockfd1, sockfd2: valid sockets
 */
void bridge_loop(int sockfd1, int sockfd2) {
	fd_set set;
	struct timeval timeout;

	int fromfd, tofd;
	while(1) {
		// set for select usage must be initialized before each select call
		// set manages which file descriptors are being watched
		FD_ZERO (&set);
		FD_SET (sockfd1, &set);
		FD_SET (sockfd2, &set);

		// same for timeout
		// max time waiting, 3 seconds, 0 microseconds
		timeout.tv_sec = servers.time; // 3 seconds
		timeout.tv_usec = 0;

		// select return the number of file descriptors ready for reading in set
		switch (select(FD_SETSIZE, &set, NULL, NULL, &timeout)) {
			case -1:
				// error
				close(sockfd1);
				return;
			case 0:
				// timed out
				dprintf(sockfd1, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
				close(sockfd1);
				return;
			default:
				if (FD_ISSET(sockfd1, &set)) {
					fromfd = sockfd1;
					tofd = sockfd2;
				} else if (FD_ISSET(sockfd2, &set)) {
					fromfd = sockfd2;
					tofd = sockfd1;
				} else {
					printf("this should be unreachable\n");
					return;
				}
		}
		if (bridge_connections(fromfd, tofd) <= 0)
			return;
	}
}

void* threadFunction(void* voidacceptfd){
	servers.servicing++;
	int acceptfd = *((int*) voidacceptfd);
	// pick best server
	uint16_t best = 0;
	int leastRequests = REQUEST_MAX;
	int leastFails = REQUEST_MAX;
	for(uint16_t i = 0; i < servers.numServers; i++){
		if(servers.serverStats[i].alive == false) // cannot use a dead server
			continue;
		if(servers.serverStats[i].requests < leastRequests){
			leastRequests = servers.serverStats[i].requests;
			best = i;
			leastFails = servers.serverStats[i].fails;
		}else if((servers.serverStats[i].fails < leastFails) && (servers.serverStats[i].requests == leastRequests)){
			leastRequests = servers.serverStats[i].requests;
			best = i;
			leastFails = servers.serverStats[i].fails;
		}
	}
	servers.serverStats[best].alive = true;
	if((best == 0) && (servers.serverStats[best].alive == false)){
		dprintf(acceptfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
		return NULL;
	}
	int connfd;
	if ((connfd = client_connect(servers.serverStats[best].port)) < 0){
		dprintf(acceptfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
		servers.serverStats[best].alive = false;
		return NULL;
	}

	bridge_loop(connfd, acceptfd);
	
	close(connfd);
	servers.servicing--;
	servers.serverStats[best].requests++;
	servers.totRequests++;
	pthread_cond_signal(&servers.wait);
	pthread_cond_signal(&servers.full);
	return NULL;
}

int main(int argc,char **argv) {
	int listenfd, acceptfd;
	uint16_t listenport;
	char* charThreads = "4"; // default number of threads is 4
	char* charRequests = "5";
	servers.time = 3;
	servers.servicing = 0;
	uint8_t numRequests;
        uint8_t numThreads;
        char* port;
        int flag;
        while((flag = getopt(argc, argv, "R:N:")) != ERROR){
                switch(flag){
                        case 'R':
                                charRequests = optarg; // number of requests inbetween checks in char form
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

        port = argv[optind]; // load balancer port
	servers.port = atoi(port);
	if((servers.port > 0) && (servers.port <= 99999)){
		// error 500, must be correct port size
	}

	optind++; // start of 
	servers.numServers = argc - optind;

	if(servers.numServers < 1){
		// error 500, must supply ports
	}
	struct serverStats serverStatsTemp[servers.numServers];
	memset(&serverStatsTemp, 0, sizeof(struct serverStats) * servers.numServers);
	servers.serverStats = serverStatsTemp;
	for(uint16_t i = 0; i < servers.numServers; i++){
		servers.serverStats[i].port = atoi(argv[optind + i]);
		if((servers.serverStats[i].port > 0) && (servers.serverStats[i].port <= 99999)){
			// error 500, must be correct port size
		}
		servers.serverStats[i].alive = false;
	}
	
	pthread_t serviceThread;
	if(pthread_create(&serviceThread, NULL, healthCheck, NULL) != 0){
		//error
	}
	
        numRequests = atoi(charRequests); // convert char* to int for number of threads
	servers.numRequests = numRequests;
        numThreads = atoi(charThreads); // convert char* to int for number of threads

	if(pthread_mutex_init(&servers.socketMutex, NULL) != 0){
		// error
	}
	if(pthread_mutex_init(&servers.healthMutex, NULL) != 0){
		// error
	}
        if(pthread_cond_init(&servers.full, NULL) != 0){
                //error, not clients fault
        }
        if(pthread_cond_init(&servers.wait, NULL) != 0){
                //error, not clients fault
        }

	if (argc < 3) {
		printf("missing arguments: usage %s port_to_connect port_to_listen", argv[0]);
		return 1;
	}

	// Remember to validate return values
	// You can fail tests for not validating
	listenport = servers.port; // load balancer
	if ((listenfd = server_listen(listenport)) < 0)
		err(1, "failed listening");

	/*
           Connecting with a client
           */
        struct sockaddr client_addr;
        socklen_t client_addrlen;
	
	servers.totRequests = servers.numRequests;
	pthread_cond_signal(&servers.wait);
	servers.totRequests = 0;

	while(true){
		if ((acceptfd = accept(listenfd, &client_addr, &client_addrlen)) < 0){
			continue; // if failed to connect, just skip
		}

		pthread_mutex_lock(&servers.socketMutex);

		while(numThreads == servers.servicing){ // cannot serve more than what number threads is set to
			pthread_cond_wait(&servers.full, &servers.socketMutex);
		}
		
		pthread_t thread;
		if(pthread_create(&thread, NULL, threadFunction, (void*)&acceptfd) != 0){
			//error
		}

		pthread_mutex_unlock(&servers.socketMutex);
	}
	return EXIT_SUCCESS;
}
