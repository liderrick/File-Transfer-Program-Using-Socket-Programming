/* Derrick Li - CS 372 Section 400 - Aug. 12, 2018
 * Project 2 - File 1 of 2: ftserver.c
 *
 * A server file transfer program. Pairs with ftclient.py.
 * USAGE: ftserver server_port
 *
 * Citation: Used boilerplate code 'server.c' from 'CS 344 - Operating Systems' course at
 * 			 Oregon State University (Winter 2018, Professor Benjamin Brewster)
 *     		 (https://oregonstate.instructure.com/courses/1662153/files/69465521/)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>

// Constants
#define CONNECTION_QUEUE_SIZE	5				// Maximum connections allowed to queue in listen() call
#define	MAX_SEND_BUFFER_SIZE	1025			// Maximum byte size of send/receive buffer: 1024 data bytes + 1 byte for null-terminator

// Global variables
// Buffers
	char sendBuffer[MAX_SEND_BUFFER_SIZE];				// Send buffer
	char recvBuffer[MAX_SEND_BUFFER_SIZE];				// Receive buffer
	char command[MAX_SEND_BUFFER_SIZE];					// Buffer to hold command
	char filename[MAX_SEND_BUFFER_SIZE];				// Buffer to hold filename (path + file)
	char actualFile[MAX_SEND_BUFFER_SIZE];				// Buffer to hold filename (just file)
	char pathToFile[MAX_SEND_BUFFER_SIZE];				// Buffer to hold path to file (path)
	char hostname[MAX_SEND_BUFFER_SIZE];				// Buffer for hostname
	char portNumberDataString[MAX_SEND_BUFFER_SIZE];	// Port number for data connection in string format
	char dirBuffer[MAX_SEND_BUFFER_SIZE];				// Buffer for directory listing
// Counters
	int charsRead = 0;									// Counter for number of bytes received from socket
	int charsWritten = 0;								// Counrter for number of bytes written to socket
	int readIntoBufferSize = 0;							// Counter for number of bytes read from file into send buffer
	int sentFromBufferSize = 0;							// Counter for number of bytes able to be sent from send buffer
	int totalSendSize = 0;								// Cumulative number of bytes sent
// Variables need for control socket connection
	int listenSocketFDCtrl,								// Listening socket file descriptor for control connections
		establishedConnectionFDCtrl,					// Socket file descriptor created for accepted control connection
		portNumberCtrl;									// Port number used for control socket
	socklen_t sizeOfClientInfoCtrl;						// Size of struct hostent for client (in control connection)
	struct sockaddr_in serverAddressCtrl,				// Server address struct for control connection
						clientAddressCtrl;				// Client address struct for control connection
	struct hostent* clientHostInfo;						// Host info for client
// Variables need for data socket connection
	int listenSocketFDData,								// Listening socket file descriptor for data connection
		establishedConnectionFDData,					// Socket file descriptor created for accepted data connection
		portNumberData;									// Port number for data connection
	socklen_t sizeOfClientInfoData;						// Size of struct hostent for client (in data connection)
	struct sockaddr_in serverAddressData,				// Server address struct for data connection
						clientAddressData;				// Client address struct for data connection
// Directory
	DIR *dir;											// Directory pointer
	struct dirent *ent;									// Struct for entry in directory

// Error function used for reporting issues
void error(const char *msg) {
	perror(msg);
	exit(0);
}

// Create control socket and listen for incoming connections
void startUp(int argc, char *argv[]) {
	// Set up the address struct for the control socket on the server (this process)
	memset((char *)&serverAddressCtrl, '\0', sizeof(serverAddressCtrl)); 	// Clear out the address struct
	portNumberCtrl = atoi(argv[1]); 										// Get the port number, convert to an integer from a string
	serverAddressCtrl.sin_family = AF_INET; 								// Use IPv4 address family
	serverAddressCtrl.sin_port = htons(portNumberCtrl); 					// Store the port number
	serverAddressCtrl.sin_addr.s_addr = INADDR_ANY; 						// Any address is allowed for connection to this process

	// Set up the listening socket for control connection
	listenSocketFDCtrl = socket(AF_INET, SOCK_STREAM, 0); 					// Create the control socket
	if (listenSocketFDCtrl < 0) error("ERROR opening socket - control");

	// Enable the control socket to begin listening
	if (bind(listenSocketFDCtrl, (struct sockaddr *) &serverAddressCtrl, sizeof(serverAddressCtrl)) < 0) 	// Connect control socket to port
		error("ERROR on binding - control");
	listen(listenSocketFDCtrl, CONNECTION_QUEUE_SIZE);		// Flip the socket on - it can now receive up to CONNECTION_QUEUE_SIZE connections
}

// Establish control connection with client on control port
void establishControlConnection() {
	// Accept a connection, blocking if one is not available until one connects
	sizeOfClientInfoCtrl = sizeof(clientAddressCtrl); // Get the size of the address for the client that will connect
	establishedConnectionFDCtrl = accept(listenSocketFDCtrl, (struct sockaddr *) &clientAddressCtrl, &sizeOfClientInfoCtrl); // Accept
	if (establishedConnectionFDCtrl < 0) error("ERROR on accept - control");

	// Get hostname from IP address, truncate hostname to first occurence of '.', and display truncated hostname.
	clientHostInfo = gethostbyaddr(&clientAddressCtrl.sin_addr, sizeof(struct in_addr), AF_INET);	// Get client info
	memset(hostname, '\0', sizeof(hostname));														// Clear hostname buffer
	strcpy(hostname, clientHostInfo->h_name);														// Copy in h_name from client info
	char *p = strchr(hostname, '.');																// Locate the first occurence of '.'
	if(p != NULL) *p = '\0';																		// Put '\0' to truncate string
	printf("\nConnection from %s.\n", hostname);													// Print connection status message
}

// Receive request from client and parse. Return 1 if invalid request received; else return 0;
int handleRequest() {
	// Clear buffers
	memset(command, '\0', sizeof(command));
	memset(filename, '\0', sizeof(filename));
	memset(portNumberDataString, '\0', sizeof(portNumberDataString));

	// Receive command, filename (optional), and portNumberData
	memset(recvBuffer, '\0', sizeof(recvBuffer)); 											// Clear out the receive buffer
	charsRead = recv(establishedConnectionFDCtrl, recvBuffer, sizeof(recvBuffer) - 1, 0); 	// Read data from the socket, leaving room for '\0' at end
	if (charsRead < 0) error("ERROR reading from socket");									// Check for read errors

	// Extract command from received string
	sscanf(recvBuffer, "%s", command);

	// Extract additional parameters based on command;
	// if "-l" command, extract only portNumberData;
	// else if "-g" command, extract filename and portNumberData;
	// else return an error message to client.
	if(strcmp(command, "-l") == 0) {
		sscanf(recvBuffer, "%*s %d", &portNumberData);													// Extract portNumberData
		printf("\nList directory requested on port %d.\n", portNumberData);								// Print status message
	} else if (strcmp(command, "-g") == 0) {
		sscanf(recvBuffer, "%*s %s %d", filename, &portNumberData);										// Extract filename and portNumberData
		printf("\nFile \"%s\" requested on port %d.\n", filename, portNumberData);						// Print status message
	} else {
		printf("\nInvalid request received.\n");														// Print status message
		memset(sendBuffer, '\0', sizeof(sendBuffer));													// Clear send buffer
		strcpy(sendBuffer, "ERR Error: Invalid command. Possible commands:\"-l\", \"-g\".");			// Copy error message into send buffer

		charsWritten = send(establishedConnectionFDCtrl, sendBuffer, strlen(sendBuffer), 0); 			// Write to control socket
		if (charsWritten < 0) error("ERROR writing to socket");											// Check for errors
		if (charsWritten < strlen(sendBuffer)) printf("WARNING: Not all data written to socket!\n");	// Check all data has been written

		close(establishedConnectionFDCtrl); 															// Close the connection socket to client
		return 1;
	}
	return 0;
}

// Establish data connection with client on data port
void establishDataConnection() {
	// Build status message, "OK portNumberData", to inform client to connect to data port
	sprintf(portNumberDataString, "%d", portNumberData);					// Convert portNumberData back to string
	memset(sendBuffer, '\0', sizeof(sendBuffer));							// Clear send buffer
	strcpy(sendBuffer, "OK ");												// Build "OK portNumberData" string
	strcat(sendBuffer, portNumberDataString);

	// Set up the address struct for the data socket on the server
	memset((char *) &serverAddressData, '\0', sizeof(serverAddressData)); 	// Clear out the address struct
	serverAddressData.sin_family = AF_INET;									// Use IPv4 address family
	serverAddressData.sin_port = htons(portNumberData);						// Store the portNumberData
	serverAddressData.sin_addr.s_addr = INADDR_ANY;							// Any address is allowed for connection to this process

	// Set up the listening socket for data connection
	listenSocketFDData = socket(AF_INET, SOCK_STREAM, 0);					// Create the data socket
	if(listenSocketFDData < 0) error("Error opening socket - data");

	// Enable the data socket to begin listening
	if(bind(listenSocketFDData, (struct sockaddr *) &serverAddressData, sizeof(serverAddressData)) < 0) 		// Connect data socket to port
		error("Error on binding - data");
	listen(listenSocketFDData, CONNECTION_QUEUE_SIZE); 		// Flip the socket on - it can now receive up to CONNECTION_QUEUE_SIZE connections

	// Send "OK portNumberData" status message to client on control connection
	charsWritten = send(establishedConnectionFDCtrl, sendBuffer, strlen(sendBuffer), 0); 				// Write to control socket
	if (charsWritten < 0) error("ERROR writing to socket");												// Check for errors
	if (charsWritten < strlen(sendBuffer)) printf("WARNING: Not all data written to socket!\n");		// Check all data has been written

	// Client connects to data socket
	sizeOfClientInfoData = sizeof(clientAddressData); // Get the size of the address for the client that will connect
	establishedConnectionFDData = accept(listenSocketFDData, (struct sockaddr *) &clientAddressData, &sizeOfClientInfoData); 	// Accept
	if(establishedConnectionFDData < 0) error("Error on accept - data");
}

// Perform recursive directory look and add all regular files (with appropriate directory paths) to directory buffer
void recursiveDirectoryLook(char *currentDir, DIR *dir) {
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL) {										// Look through all files
			if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)		// Skip entry if "." or ".."
				continue;
			if(ent->d_type == DT_REG) {												// If entry is a regular file, add to dirBuffer
				strcat(dirBuffer, currentDir);										// Concatenate directory path to file
				strcat(dirBuffer, "/");												// Concatenate "/"
				strcat(dirBuffer, ent->d_name);										// Concatenate file name to directory buffer
				strcat(dirBuffer, "\n");											// Concatenate newline
			}
			if(ent->d_type == DT_DIR) {												// If entry is a directory, recursion
				// Build directory path starting at original root "."
				char *cDir = calloc(strlen(currentDir) + strlen(ent->d_name) + 2, sizeof(char));		// Extra 2 spaces for "/" and null-terminator
				strcat(cDir, currentDir);											// Build directory path to target folder
				strcat(cDir, "/");
				strcat(cDir, ent->d_name);
				DIR *rDir = opendir(cDir);											// Open up target folder
				recursiveDirectoryLook(cDir, rDir);									// Recursive look into folder
			}
		}
		closedir(dir);																// Close directory
	}
}

// Send current directory listing to client
void sendDirectoryListing() {
	// Clear directory buffer
	memset(dirBuffer, '\0', sizeof(dirBuffer));

	// Open current directory and concatenate names of entries to dirBuffer
	// Citation: https://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c
	dir = opendir(".");
	recursiveDirectoryLook(".", dir);

	// Send into control socket "OK-DISPLAY" message along with byte size of directory listing client should expect to receive
	memset(sendBuffer, '\0', sizeof(sendBuffer));													// Clear send buffer
	sprintf(sendBuffer, "OK-DISPLAY %ld", strlen(dirBuffer));
	charsWritten = send(establishedConnectionFDCtrl, sendBuffer, strlen(sendBuffer), 0); 			// Write to control socket
	if (charsWritten < 0) error("ERROR writing to socket");		// Check for errors
	if (charsWritten < strlen(sendBuffer)) printf("WARNING: Not all data written to socket!\n");	// Check all data has been written

	// Loop until all of directory listing has been sent. Increment pointer and decrement size to ensure data is sync.
	totalSendSize = 0;												// Reset cumulative counter
	while(totalSendSize < strlen(dirBuffer)) {
		charsWritten = send(establishedConnectionFDData, dirBuffer + totalSendSize, strlen(dirBuffer) - totalSendSize, 0); 	// Write to data socket
		if (charsWritten < 0) error("ERROR writing to socket");		// Check for errors
		totalSendSize += charsWritten;								// Increment cumulative counter
	}

	// Print status message
	printf("Sending directory contents to %s:%d\n", hostname, portNumberData);
}

// Send requested file to client
void sendFile() {

	// Parse the filename from the supplied path and open the appropriate directory
	memset(actualFile, '\0', sizeof(actualFile));			// Clear out actualFile buffer
	memset(pathToFile, '\0', sizeof(pathToFile));			// Clear out actualFile buffer

	strcpy(pathToFile, filename);							// Copy full path + filename into pathToFile buffer

	char *p = strrchr(pathToFile, '/');						// Find last occurrence of '/' in pathToFile
	if(p != NULL) {											// If found, then a path is supplied
		strcpy(actualFile, p + 1);							// Extract the actual filename from path
		*p = '\0';											// Truncate the path to just folders
		dir = opendir(pathToFile);							// Open the path directory
	} else {												// Else, only the file name was supplied
		strcpy(actualFile, filename);						// Just copy the file name over to actual filename
		dir = opendir(".");									// Open current directory
	}

	// Look in current directory and check if file exists
	// Citation: https://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c
	int fileInDir = 0;										// Flag for existence of file
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL) {				// Look through all files in folder
			if (strcmp(ent->d_name, actualFile) == 0) {		// If file exists, set fileInDir flag
				fileInDir = 1;
				break;
			}
		}
		closedir(dir);										// Close directory
	}

	// If file not found, send error message to client on control connection;
	//		else send file to client on data connection.
	if(!fileInDir) {				// Send error message to client if file doesn't exists
		memset(sendBuffer, '\0', sizeof(sendBuffer));													// Clear send buffer
		strcpy(sendBuffer, "ERR-DISPLAY FILE NOT FOUND");												// Put error message in send buffer
		charsWritten = send(establishedConnectionFDCtrl, sendBuffer, strlen(sendBuffer), 0); 			// Write to control socket
		if (charsWritten < 0) error("ERROR writing to socket");											// Check for errors
		if (charsWritten < strlen(sendBuffer)) printf("WARNING: Not all data written to socket!\n");	// Check all data has been written

		printf("File not found. Sending error message to %s:%d\n", hostname, portNumberCtrl);			// Print status message
	} else {						// Send file if it exists
		// Get length of file
		// Citation: https://stackoverflow.com/questions/21074084/check-text-file-length-in-c
		FILE *inputFile;  										// Input file stream
		int fileCharLength = 0;									// File length
		int status;												// Returned status from fseek function call
		char readChar;											// Read-in char from file

		inputFile = fopen(filename, "r");  						// Open up actualFile for reading
		if (inputFile == NULL) error("Error opening file");

		status = fseek(inputFile, 0, SEEK_END);  				// Move pointer to end of file
		if(status < 0) error("Error seeking in file");

		fileCharLength = (int) ftell(inputFile);  				// Return measured bytes from beginning of file
		if(fileCharLength < 0) error("Error determining length of file");

		// Send into control socket "OK-SAVE" message along with byte size of file for client to expect
		memset(sendBuffer, '\0', sizeof(sendBuffer));			// Clear send buffer
		sprintf(sendBuffer, "OK-SAVE %d", fileCharLength);
		charsWritten = send(establishedConnectionFDCtrl, sendBuffer, strlen(sendBuffer), 0); 			// Write to control socket
		if (charsWritten < 0) error("ERROR writing to socket");											// Check for errors
		if (charsWritten < strlen(sendBuffer)) printf("WARNING: Not all data written to socket!\n");	// Check all data has been written

		status = fseek(inputFile, 0, SEEK_SET);  				// Move pointer to start of file
		if(status < 0) error("Error seeking in file");

		totalSendSize = 0;										// Reset counter for cumulative number of bytes sent

		// Loop until all data bytes have been sent into data socket
		while(totalSendSize < fileCharLength) {
			// Reset buffer size counters at the beginning of each cycle
			readIntoBufferSize = 0;								// Counter for number of bytes read from file into send buffer
			sentFromBufferSize = 0;								// Counter for number of bytes able to be sent from send buffer

			memset(sendBuffer, '\0', sizeof(sendBuffer));		// Clear send buffer

			// Fill the send buffer with MAX_SEND_BUFFER_SIZE - 1 chars from the inputfile stream
			int i;
			for(i = 0; i < MAX_SEND_BUFFER_SIZE - 1; i++) {
				readChar = fgetc(inputFile);			// Get a char from the file; pointer is auto-incremented
				if(readChar == EOF)						// Break loop if end-of-file is reached
					break;
				strncat(sendBuffer, &readChar, 1);		// Concatenate char onto send buffer
				readIntoBufferSize++;					// Increment readIntoBufferSize counter
			}

			// Send data into data socket. Loop to ensure all data has been sent. Increment pointer and decrement size to ensure data is sync.
			while(sentFromBufferSize < readIntoBufferSize) {
				charsWritten = send(establishedConnectionFDData, sendBuffer + sentFromBufferSize, strlen(sendBuffer) - sentFromBufferSize, 0); 	// Write to data socket
				if (charsWritten < 0) error("ERROR writing to socket");					// Check for errors
				sentFromBufferSize += charsWritten;										// Increment sentFromBufferSize counter
			}
			totalSendSize += sentFromBufferSize;										// Increment cumulative counter
		}

		fclose(inputFile);																// Close inputfile stream

		printf("Sending \"%s\" to %s:%d\n", actualFile, hostname, portNumberData);		// Print status message
	}
}

int main(int argc, char *argv[]) {
	// Check usage and args
	if (argc < 2) {
		fprintf(stderr,"USAGE: %s server_port\n", argv[0]);
		exit(1);
	}

	// Create control socket and listen for incoming connections
	startUp(argc, argv);

	// Loop until SIGINT received
	while(1)
	{
		// Print status message
		printf("\n   *** Server open on %d *** \n", portNumberCtrl);

		// Accept incoming control connection request and establish control connection on control port
		establishControlConnection();

		// Receive request from client and parse. 1 is returned if invalid request received; else 0 is returned
		if(handleRequest()) continue;

		// Establish data connection with client on data port
		establishDataConnection();

		// If command is "-l", send directory listing;
		// else if command is "-g", send file if file exists, otherwise send error message
		if(strcmp(command, "-l") == 0) {					// Send directory listing to client
			sendDirectoryListing();
		} else if (strcmp(command, "-g") == 0) {			// Send requested file to client
			sendFile();
		}

		// Close data connection and data listening socket on server.
		// Control connection socket will be closed by client.
		// Server's listening socket for control remains open.
		close(establishedConnectionFDData);			// Close data connection socket
		close(listenSocketFDData);					// Close data listening socket
	}
	return 0;
}
