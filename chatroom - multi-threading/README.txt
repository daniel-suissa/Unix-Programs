


Running the client and the server

Server:
	 - run ./server to start up the server on default port 5197.
	 - run ./server PORT_NUM to start the server on port PORT_NUM
Client 
	- run ./client NAME to start up the client. A client name must be provided
	- by default, the client will try to connect to localhost on port 5197
	- run ./client NAME ADDRESS to have the client connect to ADDRESS on the default port
	 - run ./client NAME ADDRESS PORT_NUM to have the client connect to ADDRESS on port PORT_NUM

Code that was copied from the single threaded chat:

	server:
		most of the `getPassiveSocket` function
	client:
		most of the `converse` function
