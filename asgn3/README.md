 * Aidan Smith, aipsmith@ucsc.edu            
 * CSE 130, Spring Quarter, June 8th, 2020 
 * Assignment 3: README.md                   

 Usage guide for loadbalancer:

	1. Call "make" to create the executable file.

	2. Call "make clean" to remove the object file.

	3. Call "./loadbalancer [args]" to run the httpserver.

		3a. args include (in any order; separated by spaces):

			3ai.   The load balancer port number, a number 1 to 99999.
			       This is where the client will make the connection to the 
			       server. This is required. This is also required to come 
			       before any other port numbers.

			3aii.  The httpserver port numbers, numbers from 1 to 99999. This
			       is where the load balancer will connect to in order to relay
			       the client's request and the httpserver's response. At least
			       one is required, but any number can be supplied. These must
			       come after the port number for the load balancer.

			3aiii. (Optional) The number of threads to run on the server.
			       Done by adding "-N (number)". -N is the flag, and number is 
			       the number of threads you would like to add to the server.

			3aiv.  (Optional) The number of requests before a health check is
			       performed on the load balancer. This process is explained in
			       the design document. By adding "-R (number)" you can cause
			       health checks to occur every (number) requests.

	4. Before you can make any requests, you must have at least one httpserver running.
	   You must open them using the criteria for assignment 2. Make sure you open them
	   with the matching port to the ones given in the previous step. You can open the
	   httpservers before even starting calling make on load balancer.

	5. Open a new terminal and call curl on localhost with the load balancer port.

		5a. If you wanted to GET a file from the server, get the file content, 
		    you would call this a command using this template: 
		    "curl localhost:[port]/[filename]"

		5b. If you wanted to HEAD a file from the server, print out the header, 
		    then you would use call this command using this template:
		    "curl -I localhost:[port]/[filename]"

		5c. If you wanted to PUT a file onto the server, save a copy of your file 
		    onto the server, then you would call this command using this template:
		    "curl -T [file to be read] localhost:[port]/[file to be saved]"

		5d. If you wanted to perform a health check on the server, which tells you
		    how many logs have been made, and how many of those have been failures:
		    "curl localhost:[port]/healthcheck"

		5e. Any other commands will either be flagged as incorrect and logged, or
		    caught by curl.

	6. When satisfied with load balancer, call "make spotless" to remove the executable.

 Note: When errors occur with the program, they could happen after an OK or CREATED status 
       message is sent to the client. This causes the client to get the "okay" signal, but 
       then nothing happens. This is part of the program and is unavoidable as an error may 
       occur after a status message is sent.
