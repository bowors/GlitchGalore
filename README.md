# Multi-Process Web Server

This program demonstrates how to use multiple processes and a named pipe to implement a simple web server that can handle HTTP requests and download files from URLs.

## Overview

The program consists of three processes:

1. **Process 1** runs a web server using libmicrohttpd and listens for HTTP requests. When it receives a POST request at a `/hook` endpoint, it writes a message to the named pipe to notify process 2.
2. **Process 2** reads messages from the named pipe and uses a linked list of key-value pairs to look up URLs associated with the messages. When it finds a URL, it uses libcurl to download the file from the URL and prints the contents of the downloaded file to the console.
3. **Process 3** runs an ssh command to forward a remote port to the local web server.

The program uses a named pipe to communicate between processes 1 and 2. The named pipe is created by process 1 and opened for reading by process 2. When process 1 receives a POST request at a `/hook` endpoint, it writes a message to the named pipe. Process 2 reads messages from the named pipe and processes them.

The program also uses a linked list of key-value pairs to associate messages with URLs. The list is created by process 1 and passed as an argument to process 2. When process 2 receives a message from process 1, it uses the list to look up the URL associated with the message.

## Usage

To compile the program, you will need to have libmicrohttpd and libcurl installed on your system. You can then compile the program using the following command:

```
gcc -o myprogram myprogram.c -lmicrohttpd -lcurl
```

To run the program, you need to specify an HTML file and one or more URLs as command line arguments. For example:

```
./myprogram myfile.html https://example.com https://google.com https://bing.com
```

In this example, the program will read the HTML content from `myfile.html` and add the specified URLs to the list. In process 2, it will use these URLs to download files when it receives messages from process 1.

When you run the program, it will start the web server and listen for HTTP requests on port `5150`. You can send POST requests to `/hook` endpoints using a tool like `curl` or `Postman`. For example:

```
curl -X POST http://localhost:5150/hook/1
```

When you send a POST request to a `/hook` endpoint, process 1 will write a message to the named pipe and notify process 2. Process 2 will read the message from the named pipe, look up the URL associated with the message in the list, and download the file from the URL using libcurl. The contents of the downloaded file will be printed to the console.
