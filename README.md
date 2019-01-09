# File Transfer Program Using Socket Programming

This is a pair of file transfer programs capable of sending and receiving text files in a client-server manner. A client can connect to the server and request either the directory listing or a text file be sent. The client `ftclient.py` is coded in Python, and the server `ftserver.c` is coded in C. Both programs uses the socket API.

Boilerplate code from Oregon State University, CS 344 - Winter 2018 and Computer Networking: A Top-Down Approach, 7th Edition by Jim Kurose and Keith Ross were used in coding this project.

## Instructions

### Notes
* The file transfer server `ftserver.exe` must be started prior to any file transfer client `ftclient.py`.
* There are two possible commands the client can request from the server: `-l` to retrieve the directory listing, and `-g` to get a text file. All other commands are ignored.
* If using `-l`, a filename should not be supplied.
* If using `-g`, a full filename (including the file extension, if present) must be provided as a command line argument. If a file with the same name already exists in the directory, then `ftclient` will prepend `copy_` to the downloaded file.
* A file can be requested from other directories besides root. In place of `filename`, supply a relative path. For example, `./folder1/folder2/nestedfile.txt`. A security warning: It is also possible to navigate into the parent directories.

### Running ftserver.exe
1. Run `make` to compile `ftserver.exe` from `ftserver.c`.
2. Run `ftserver.exe <server_port>` to start the file transfer server on the specified server port number. For example, `ftserver.exe 12345`.
  * In the unlikely event that the port is in use, use a different port number in the range of `0 - 65535`.
3. The server is only capable of processing a single request at a time. If there are multiple concurrent requests, the server will process them in a FIFO manner.
4. Exit the file transfer server by sending SIGINT `[Ctrl]+C`.

### Running ftclient.py
Run `python3 ftclient.py <server_host> <server_port> <command> [filename] <data_port>` to send a request to the server.
* The `server_host` should be the name of the machine hosting the file transfer server `ftserver.exe`. If you are running locally, it is most likely `localhost` or `127.0.0.1`.
* The `server_port` is the port number the file transfer server is running on and thus expecting requests from. In the example above, the `server_port` would be `12345`.
* The `command` can either be `-l` or `-g`. In the case of `-l`, the `filename` argument can be ignored and a filename shouldn't be supplied. However, if the `command` is `-g`, then a full `filename` must be supplied.
* The `data_port` is the chosen port number the server should use to send the data to the client. It **must** be different from `server_port`. The server uses this port number to send back both the directory listing and data from text files.
* Some valid examples:
  * `python3 ftclient.py localhost 12345 -l 50001`
  * `python3 ftclient.py localhost 12345 -g myfile.txt 65431`
  * `python3 ftclient.py localhost 12345 -g ./myfile.txt 65431`
  * `python3 ftclient.py localhost 12345 -g ./folder1/folder2/nestedfile.txt 60001`