 # Derrick Li - CS 372 Section 400 - Aug. 12, 2018
 # Project 2 - File 2 of 2: ftclient.py
 #
 # A client file transfer program. Pairs with executable compiled from ftserver.c.
 # USAGE: python3 ftclient.py server_host server_port command [filename] data_port
 # 			A filename is required if the command is "-g".
 #
 # Citation: Used boilerplate code 'TCPClient.py' from Computer Networking: A Top-Down Approach, 7th Edition by
 # 			 Jim Kurose and Keith Ross (CH 2 - Application Layer, Section 2.7 - Socket Programming, p166).

import sys
import socket
import os

# Constants
MAX_RECV_SIZE 			= 1024			# Max receive buffer byte size

# Global variables
serverName				= 0				# Server's host info
serverPort				= 0				# Server's control port
clientControlSocket		= 0				# Client's control connection with server
serverDataPort			= 0				# Server's data port
clientDataSocket		= 0				# Client's data connection with server

# Check usage and args are correct. A minimum of 5 args are needed; or if the command is "-g", then a minimum of 6 args are needed.
def checkUsage():
	if len(sys.argv) < 5 or (sys.argv[3] == "-g" and len(sys.argv) < 6):
		print("USAGE: python3 " + sys.argv[0] + " server_host server_port command [filename] data_port")
		if len(sys.argv) > 3 and sys.argv[3] == "-g":			# len(sys.argv) > 3 is needed to short-circuit if number of args is < 3
			print("--> filename is required if command is \"" + sys.argv[3] + "\".")
		sys.exit(1)

# Create control connection with server
def initiateContact():
	global serverName, serverPort, clientControlSocket
	serverName = socket.gethostbyname(sys.argv[1])								# Get host info from server_host command-line argument
	serverPort = int(sys.argv[2])												# Get server_port number from arguments
	clientControlSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)		# Create TCP socket for control connection
	clientControlSocket.connect((serverName, serverPort))						# Connect to server and establish control connection

# Create request from client's command-line arguments and send to server
def makeRequest():
	global clientControlSocket
	sendInstruction = ""
	for i in range(3, len(sys.argv)):											# Create string with command, [filename], and data_port
		sendInstruction += sys.argv[i] + " "									# Concatenate the arguments to accumulating string
	clientControlSocket.send(sendInstruction.encode())							# Send command, [filename], and data_port to server

# Create data connection with server
# Input: data port number the server has created a socket on and started listening to
def createDataConnection(data_port):
	global serverName, serverDataPort, clientDataSocket
	serverDataPort = int(data_port)												# Store the data port number
	clientDataSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)		# Create TCP socket for data connection
	clientDataSocket.connect((serverName, serverDataPort))						# Connect to same server host, but different port for data connection

# Receive directory from server and display on screen.
# Input: size of directory listing in bytes
def receiveDirectory(directory_size):
	global serverDataPort, clientDataSocket
	print("Receiving directory structure from " + sys.argv[1] + ":" + str(serverDataPort))
	receivedDirectory = ""
	while len(receivedDirectory) < directory_size:								# Loop until all bytes have been received
		receivedDirectory += clientDataSocket.recv(MAX_RECV_SIZE).decode()		# Concatenate received data to accumulating buffer
	print("\n" + receivedDirectory)												# Print directory to screen

# Receive file from server and save to disk. If duplicate file name, prepend "copy_" to file until no longer duplicate name.
# Input: size of file in bytes
def receiveFile(file_size):
	global serverDataPort, clientDataSocket
	print("Receiving \"" + sys.argv[4] + "\" from " + sys.argv[1] + ":" + str(serverDataPort))

	# Check if file exists, prepend "copy_" until not duplicate name; else just use supplied filename
	outputFileName = sys.argv[4].split('/')[-1]			# Split the filename argument at the '/'s, then take last element
	while os.path.isfile(outputFileName):
		outputFileName = "copy_" + outputFileName

	print("Saving to current directory as \"" + outputFileName + "\".")

	# Open output file for writing
	outputFile = open(outputFileName, "w")

	# Save received bytes into file
	recvBytes = 0														# Counter to keep track of total bytes received
	while recvBytes < file_size:										# Loop until counted total bytes >= number of bytes expected
		recvBuffer = clientDataSocket.recv(MAX_RECV_SIZE).decode()		# Receive data, not exceeding MAX_RECV_SIZE each instance
		recvBytes += len(recvBuffer)									# Count the bytes just received and increment counter accordingly
		outputFile.write(recvBuffer);									# Write data to file
	outputFile.close()													# Close output file once all data has been written
	print("File transfer complete.")									# Display status message

# Main function
def main():
	checkUsage()							# Check for proper usage of command-line arguments
	initiateContact()						# Initiate contact with server and create control connection
	makeRequest()							# Extract arguments and send request to server

	# Receive reply from server (status code and message)
	receivedMsg = clientControlSocket.recv(MAX_RECV_SIZE).decode()

	# Split off the first word (contains the status code) of the received message from the remainder, which is the message
	(status1, msg1) = receivedMsg.split(' ', 1)

	# Status should be either "ERR" or "OK", followed by the error message or the expected data port, respectively
	if status1 == "ERR":				# If error, display error message and exit with error code
		print(msg1)							# msg1 holds the error message
		clientControlSocket.close()			# Close control connection
		sys.exit(1)							# Exit with error code 1
	elif status1 == "OK":				# Else if OK, create data connection
		createDataConnection(msg1)			# Create data connection. msg1 holds the data port number

		# Receive reply from server (status code and message)
		receivedMsg = clientControlSocket.recv(MAX_RECV_SIZE).decode()

		# Split off the first word (contains the status) of the received message from the remainder, which is the message
		(status2, msg2) = receivedMsg.split(' ', 1)

		# Status can be "ERR-DISPLAY" if file is not found,
		# 				"OK-DISPLAY" if client is requesting directory listing, or
		# 				"OK-SAVE" if client is requesting a file.
		# The msg2 will hold the error message in the case of an "ERR" status, or the
		# number of bytes the client should expected to receive in the latter two "OK" statuses.
		if status2 == "ERR-DISPLAY":						# msg2 holds 'file not found' error message
			print(sys.argv[1] + ":" + str(serverPort) + " says " + msg2)
		elif status2 == "OK-DISPLAY": 						# msg2 holds number of bytes to expect for directory listing
			receiveDirectory(int(msg2))
		elif status2 == "OK-SAVE":							# msg2 holds number of bytes to expect for file
			receiveFile(int(msg2))

	# Close control connection. Data connection will be closed by server.
	clientControlSocket.close()													# Close control connection

if __name__ == "__main__":
	main()