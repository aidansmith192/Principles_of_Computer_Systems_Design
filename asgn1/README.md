 * Aidan Smith, aipsmith@ucsc.edu            
 * CSE 130, Spring Quarter, May 5th, 2020 
 * Assignment 1: README.md                   

 Usage guide for httpserver:

	1. Call "make" to create the executable file.

	2. Call "make clean" to remove the object file.

	3. Call "./httpserver [port number]" to run the httpserver.

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

	5. When satisfied with httpserver, call "make spotless" to remove the executable.

 When errors occur with the program, they could happen after an OK or CREATED message is
 sent to the client. This causes the client to get the "okay" signal, but then nothing
 happens. This is part of the program and is unavoidable as an error may occur after a
 status message must be sent.
