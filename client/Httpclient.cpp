/*
	This is simple HTTP client which contacts the Proxy server initially for requesting web pages. It then waits for dat from proxy and writes
	it to a file.The proxy may either send its local copy or from Server if it does not have the updated page.

*/

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>		// gethostbyname()
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <string>

using namespace std;

#define FILE_NOT_FOUND "Connection refused" 
#define NO_RELATIVE_PATH "No relative path"

//Get relative path of the URL
std::string getrelativepath(std::string url);

int main(int argc, char *argv[])
{

  int port,sd;//sd is the socket descriptor of the client
  int j=1,yes=1;
//host is the ip address of server,buffer is a pointer to the string buffer,sendbuf is buffer which stores the value to be sent to server
  char *host,sendbuf[512],receivebuf[100000],filename[100000];
  int sentbytes,recbytes,pos,pos1,found = 0;
  struct sockaddr_in serveraddress;//Contains server details like port and IP address
  struct in_addr **ipaddrlist;
  struct hostent *hp,*test;//gethostbyname()
  char * url;
 const char *urlip;
  FILE *pFile;
  std::string urlstring,relativepath,tmpstring;//Strings for URL manipulation
  std::string::const_iterator it;
 
//Get command line argumentvalues for server IP,port and URL
  if(argc == 4)
    {      
      host = argv[1];
      port = atoi(argv[2]);
      url = argv[3];
    }
  else//Force exit
    {
      cout<<"\n The number of arguments entered is "<<argc-1;
      cout<<"\n Please enter the 3 arguments in ProxyServerip port URL format \n";
      exit(1); 
    }

   //Create socket
   sd = socket(AF_INET,SOCK_STREAM,0);

  //Check for failure for creating socket
  if(sd == -1)
    {
       cout<<"\n Unable to create socket\n";
       exit(1);
    }

    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

//set as 0 initially
  memset(&serveraddress,0,sizeof(serveraddress));
  serveraddress.sin_family = AF_INET;
  serveraddress.sin_port = htons(port);
  hp = gethostbyname(host);
  if(hp == NULL)
  {
     cout<<"\n Unable to get host address\n";
    exit(1);
  }
//Copy host address to server address
  bcopy(hp->h_addr,(char *)&serveraddress.sin_addr,hp->h_length);



//Establish connection to server
 if(connect(sd,(struct sockaddr *)&serveraddress,sizeof(serveraddress)) == -1)
  {
    cout<<"\n Cannot connect to server\n";
    exit(1);
  }
 
    cout<<"\n Connected to server whose address is "<<hp->h_name<<"\n";
    urlstring = url;
    pos = urlstring.find("http://");//Remove http:// from the argument field if present
    if(pos != (int)string::npos)
    {
      urlstring=urlstring.substr(pos+7);
    //  url = urlstring;
    }
	
	cout<<"\nURL string is "<<urlstring;
	
	//Find if the IP address is in domain name or 32 bit format.
	it = urlstring.begin();
    pos1 = urlstring.find(".");
		for(int i=0;i != pos1;i++,it++)
		  {
			if(std::isdigit(*it))
			  found = 1;
			else
			{
              found = 0;	
              break;
            }			  
		  
		  }
	
	//If in domain name format, get IP address using gethostbyname function
	if(found == 0)
	{
		test = gethostbyname(urlstring.c_str());
		
		 ipaddrlist = (struct in_addr **)test->h_addr_list;
		 if( *ipaddrlist != NULL )
		 {
		  urlip = inet_ntoa(**ipaddrlist);		
		}
	}
	else
	{
	//Get only  the 32 bit IP
	pos1 = urlstring.find("/");
	if(pos1 != (int)string::npos)
	{
		tmpstring = urlstring.substr(0,pos1);
		urlip = tmpstring.c_str();
	}	
	else
		urlip = urlstring.c_str();
	
	}
	
	cout<<"\n URL IP add is "<<urlip<<endl;
	
	//Separate out relative path of URL
    relativepath = getrelativepath(urlstring);
    
    strcpy(sendbuf,"GET / HTTP/1.0 ");
    //strcat(sendbuf,"Host: ");
    strcat(sendbuf,urlip);
    strcat(sendbuf,relativepath.c_str());


    cout<<"\n Sending data is "<<sendbuf;
	//Send to Proxy Server
   sentbytes = send(sd,sendbuf,strlen(sendbuf),0);

   //cout<<"\n Sent bytes is "<<sentbytes<<"\n";

   for(int i=1;i<=2;i++)
   {
   	 bzero(receivebuf,100000);
	 //Recive data from Proxy
   	 recbytes = recv(sd,receivebuf,100000,0);   	 
   	 if(j == 1)//1st receive. We get filename.
   	 {   	 	
   	 	if(strcmp(receivebuf,FILE_NOT_FOUND) == 0)
   	 	{
   	 		cout<<"\n Connection is refused for this URL. Please check if you have given the correct URL.\n";
   	 		exit(1);
   	 	}
  		cout<<"\n The response is being written to file "<<receivebuf<<endl;
   	 	pFile = fopen(receivebuf,"w");//Open file and ready for write
		strcpy(filename,receivebuf);
   	 	j++;
   	 	//memset(receivebuf,'\0',512);
   	 	cout<<"\n";
   	 	continue;
   	 }
		//Copy daya to file
   	 	fputs(receivebuf,pFile);
		
		cout<<"\nReceive Buffer: \n"<<receivebuf;
   	 
   	/* else
   	 {
   	 	fputs(receivebuf,pFile);
   	 	cout<<"\n File transfered successfully. \n";
   	 	break;
   	 }*/
   	 
   }

	fclose(pFile);
	return 0;
}

//Get relative path of the URL
std::string getrelativepath(std::string url)
{
  std::string relativepath = "/";//Get path after / symbol in URL
  int pos;
  pos = url.find("/");
  if(pos != (int)string::npos)
    {
      relativepath.append(url.substr(pos+1));  
    }
  else
  {
    relativepath = "";
  }
  return relativepath;
}
