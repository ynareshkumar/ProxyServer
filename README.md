1.  OVERVIEW
------------------------------------------------------------

Proxy Server is used to get the request from the HTTP client and passes it onto the requested server if necessary to get the page. TCP is used as the transport layer protocol. It maintains a cache of 10 pages and sends the web page from the cache if the request matches with the one in the cache. This reduces the load on the network.

---------------------------------------------------------------
* 2.  SYSTEM REQUIREMENTS 
---------------------------------------------------------------
Linux environment is required. g++ has to be installed on the operating system.

---------------------------------------------------------------
* 3.  CONTENTS OF THE PROJECT
-------Server--------------------------------------------------------
1.Proxyserver.cpp
2.Httpclient.cpp
3.Read_me.txt
4.Makefile

 
------------------------------------------------------------
* 4.  LIST OF COMMAND LINE FLAG OPTIONS
------------------------------------------------------------

Proxy Server:
- Go the server directory in the terminal
- Run the command make
- Type ./Proxyserver <Server IP> <Server Port> to start the proxy server

Httpclient :

- Go the client directory in the terminal
- Run the command make
- Type ./Proxyserver <Server IP> <Server Port> <URL to visit> to run the client

