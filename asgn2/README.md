 * Aidan Smith, aipsmith@ucsc.edu            
 * CSE 130, Spring Quarter, May 19th, 2020 
 * Assignment 2: README.md                   

 Usage guide for httpserver:

	1. Call "make" to create the executable file.

	2. Call "make clean" to remove the object file.

	3. Call "./httpserver [args]" to run the httpserver.

		3a. args include (in any order; separated by spaces):

			3ai.   The port number, a number 1 to 99999. This is where
			       the the client will make the connection to the server
			       on the server's address. This is required.

			3aii.  (Optional) The number of threads to run on the server.
			       Done by adding "-N (number)". -N is the flag, and number is 
			       the number of threads you would like to add to the server.

			3aiii. (Optional) Adding a file for logging. Done by adding
			       "-l (log file name)". -l is the flag, and log file name is
			       the name of the file that you would like to log to.

	4. Open a new terminal and call curl on localhost with the correct port.

		4a. If you wanted to GET a file from the server, get the file content, 
		    you would call this a command using this template: 
		    "curl localhost:[port]/[filename]"

		4b. If you wanted to HEAD a file from the server, print out the header, 
		    then you would use call this command using this template:
		    "curl -I localhost:[port]/[filename]"

		4c. If you wanted to PUT a file onto the server, save a copy of your file 
		    onto the server, then you would call this command using this template:
		    "curl -T [file to be read] localhost:[port]/[file to be saved]"

		4d. If you wanted to perform a health check on the server, which tells you
		    how many logs have been made, and how many of those have been failures:
		    "curl localhost:[port]/healthcheck"

		4e. Any other commands will either be flagged as incorrect and logged, or
		    caught by curl.

	5. You may check the log file during any time to see what the server has done. The
	   best way to do this would to be calling "cat (log file name)". Equivalently you
	   use the dog file created by me :). You could also open the file, just be careful
	   to not save it, as this will end the editing control of the server, and thus it
	   would have to be restarted.

	6. When satisfied with httpserver, call "make spotless" to remove the executable.

 Note: When errors occur with the program, they could happen after an OK or CREATED status 
       message is sent to the client. This causes the client to get the "okay" signal, but 
       then nothing happens. This is part of the program and is unavoidable as an error may 
       occur after a status message is sent.
