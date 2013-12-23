/*
	This is a Simple HTTP Proxy Server. It is called from the HTTP client and maintains a local cache of Size 10 to
	store the web pages. When the page is not found in cache, it contacts the HTTP server and gets the web page.
	It then takes a local copy of it for future use and sends the same to the client. It also gets the page from HTTP
	server only when the expired date and last modified date changes from previous value.
*/

#include <iostream>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>	
#include <vector>
#include <list>
#include <string>

using namespace std;

#define NUMBER_OF_CACHED_ENTRIES 10
#define BUFFER_SIZE 512
#define FILE_NOT_FOUND "Connection refused" 
#define NO_TIMESTAMP "Match not found"
#define INCORRECT_HTTP_REQUEST "Incorrect HTTP Request"

//Validate the incoming packet from Client
std::string validatepacket(char *receivebuf);
//Get relative path of the URL. Eg:index.html
std::string getrelativepath(std::string url);
//Get The IP address part of URL
std::string geturlipaddress(std::string url);
//Create a socket to contact HTTP server and return socket id
int createhttpsocket(std::string urlip);
//Check if Current date and time is greater than Expired date and time.
int fromcacheornot(std::string datestr);
//Check in cache for the URL and return the filename to send
std::string checkforurlincache(std::string receivedurl,std::string relativepath);
//Call HEAD request to get details like Last modified date.
std::string* callhead(std::string receivedurl,std::string relativepath);
//Call GET request to get web page
std::string callget(std::string receivedurl,std::string relativepath);
//Send the file to HTTP client
void sendtoclient(std::string filename,int sd);

struct cachedentry{
	std::string url;
	std::string lastmodifieddate;	
	std::string expireddate;
	std::string filename;
};

std::list<struct cachedentry> lruentries;
int nooffiles = 0;

int main(int argc, char *argv[])
{
struct sockaddr_in serveraddress;//Server address details
int port,sd,newsd,yes = 1;//sd,newsd are the socket descriptors for server and a new client, port denotes the port the server is running
const char *ipaddress;//Server IP address
struct sockaddr_in remoteaddr; // client address details
socklen_t addrlen;//length of the client address
int receivedbytes;//Return value of recv(). 
int listenresult;
char *receivebuf = (char *)malloc(512);
char sendbuf[BUFFER_SIZE];//Send buffer size
fd_set read_fds,master_fds;//file descriptors of master and temporary
int max_fd,selectresult,i;//Maximum number of connections	
std::string filenameforurl,url,relativepath,urlip;//Strings for manipulating received URL

//Get Server IP and Port as args
 if(argc == 3)
	{    
     ipaddress = argv[1]; 	
     port = atoi(argv[2]);
	}
  else//Force exit
      {
      cout<<"\n The number of arguments entered is "<<argc-1;
      cout<<"\n Please enter the 2 arguments in ServerIP Port format \n";
      exit(1); 
      }
 	
 //Create a socket 
  sd = socket(AF_INET,SOCK_STREAM,0);

//Check for any error in socket creation
  if(sd == -1)
    {
       cout<<"\n Unable to create socket";
       exit(1);
    }

  //Reuse the socket
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  
  //Initialize value to 0
  memset(&serveraddress,0,sizeof(serveraddress));
  serveraddress.sin_family = AF_INET;
  serveraddress.sin_port = htons(port);//host to network order conversion
  serveraddress.sin_addr.s_addr = inet_addr(ipaddress);
  
  //Assign the socket to a address
  if(bind(sd,(struct sockaddr *)&serveraddress,sizeof(serveraddress)) == -1)
  {
    cout<<"\n Unable to bind socket";
    exit(1);
  }
 
 //Listen for connection
  listenresult = listen(sd,10);

//Check for error in listening
  if(listenresult == -1)
   {
     cout<<"\n Unable to listen.";
     exit(1);
   }  

   //Set the socket descriptors
  FD_ZERO(&read_fds);  
  FD_ZERO(&master_fds);
  FD_SET(sd, &master_fds);
  max_fd = sd;

   while(1)
   {

  bcopy(&master_fds,&read_fds,sizeof(master_fds)); //copy the master to temporary
  selectresult = select(max_fd+1, &read_fds, NULL,NULL, NULL);
 
  if(selectresult == -1){		
     cout<<"\nError in select";
     exit(1);
   }
  //cout<<"\n Select done!";
  
  for (i=0;i<=max_fd;i++){	
    //cout<<"\n i is : "<<i<<" ISSET for i is : "<<FD_ISSET(i,&read_fds);
       if(FD_ISSET(i,&read_fds)){
       	  if(i == sd)
       	  {
			   	addrlen = sizeof remoteaddr;
					    //Accept new connection from client
			    newsd = accept(sd,(struct sockaddr *)&remoteaddr,&addrlen);
			    cout<<"Connection established for the client\n";
			     if (newsd == -1) {
			         cout<<"Error in accepting connection\n";
			      }else {
                        FD_SET(newsd, &master_fds); // add to master set
                        if (newsd > max_fd) {    // Check for the maximum value
                            max_fd = newsd;
                        }                        
                    }

	     } 
	     else
	     {
			//Receive Data from client
		      cout<<"\n Waiting for data from client";
		      receivedbytes = recv(newsd,receivebuf,512,0);
		      if(receivedbytes <= 0)
			  {

			    cout<<"\n Client has closed the connection\n";
			    FD_CLR(i,&master_fds);
			    continue;
			  }       
			  //Validate incoming packet			 
			  url = validatepacket(receivebuf);
			  //If incorrect,send that info to client
			  if(url == INCORRECT_HTTP_REQUEST)
		      	{
		      		  cout<<"\n Incorrect HTTP request.";
				      strcpy(sendbuf,INCORRECT_HTTP_REQUEST);
				      cout<<"\n Send buf "<<sendbuf;
				      send(newsd,sendbuf,strlen(sendbuf),0);
				      bzero(sendbuf,BUFFER_SIZE); 
		      	}
		      	else
		      	{
					//Get relative path and IP address part of URL
		      		  relativepath = getrelativepath(url);
					  urlip = geturlipaddress(url);

					 
					//Check if URL is present in cache
				      filenameforurl = checkforurlincache(urlip,relativepath);
				      cout<<"\n File name to send is "<<filenameforurl;
					 //If URL is not found, send info to client.
				      if(filenameforurl != FILE_NOT_FOUND)
				      	sendtoclient(filenameforurl,newsd);
				      else
				      {
				      	cout<<"\n Connection refused scenario.";
				      	strcpy(sendbuf,FILE_NOT_FOUND);
				      	cout<<"\n Send buf "<<sendbuf;
				      	send(newsd,sendbuf,strlen(sendbuf),0);
				      	bzero(sendbuf,BUFFER_SIZE);
				      }
		 		 }

		      memset(receivebuf,'\0',BUFFER_SIZE);

		      FD_CLR(newsd, &master_fds);
		      cout<<"\n";
  		 }  	  

       } 
     }
   }

	return 0;
}

//Check in cache for the URL and return the filename to send
std::string checkforurlincache(std::string receivedurl,std::string relativepath)
{
	struct cachedentry newcachedentry,lruvalue,printvalue;
	std::list<struct cachedentry>::iterator lruiterator,newiterator;
	std::string* dateinfo;
	int ispresentincache = 0;	
    int checkexpiry;
	//For 1st cache entry do not check anything. Get the web page and update cache.
	if(lruentries.empty())
	{
		newcachedentry.url = receivedurl+"/"+relativepath;
	    dateinfo= callhead(receivedurl,relativepath);
		newcachedentry.lastmodifieddate = dateinfo[0];
		newcachedentry.expireddate = dateinfo[1];		
		if(newcachedentry.lastmodifieddate == FILE_NOT_FOUND)
		{
			cout<<"404 File not found";
			return FILE_NOT_FOUND;
		}		
		newcachedentry.filename = callget(receivedurl,relativepath);
		lruentries.push_front(newcachedentry);				
		//return newcachedentry.filename;
	}
	else
	{
	//Loop through to find if URL is present in cache
		for(lruiterator = lruentries.begin();lruiterator != lruentries.end();lruiterator++)
		{
			lruvalue = *lruiterator;
			if(receivedurl+"/"+relativepath == lruvalue.url)
			{				
				//Check if expiry date is less than current date
				checkexpiry = fromcacheornot(lruvalue.expireddate);
				ispresentincache = 1;//Update the URL as prsent in cache
				if(checkexpiry)
				{
					//Send the file from local copy
					cout<<"\nFile not expired. So no HEAD request needed and fetching from cache.";
					break;
				
				}
				else
				{
					//Do a HEAD to check if last modified has changed.
					cout<<"\nFile expired. So HEAD request is needed";
					dateinfo = callhead(receivedurl,relativepath);
					lruvalue.expireddate = dateinfo[1];
				}
				if(lruvalue.lastmodifieddate != NO_TIMESTAMP)//Means Last modified field is filled in HEAD request
				{
					//dateinfo = callhead(receivedurl,relativepath);
					if(dateinfo[0] == FILE_NOT_FOUND)//Means URL not found
					{				
						cout<<FILE_NOT_FOUND;		
						return FILE_NOT_FOUND;
					}
					if(dateinfo[0] != lruvalue.lastmodifieddate)//LAst modified value has changed. So do GET.
					{						
						cout<<"\nLast modified date is changed!!.So doing a GET request.";
						lruvalue.lastmodifieddate = dateinfo[0];
						lruvalue.filename = callget(receivedurl,relativepath);		
					}
					else
						cout<<"\nLast modified date is not changed.So fetching from cache.";
				}
				else
				{
					lruvalue.filename = callget(receivedurl,relativepath);	
				}
				break;
			}
		}

	//Since present in cache, move the entry to Most recent entry.
		if(ispresentincache)
		{			
			cout<<"\n RETRIEVED FROM CACHE";
			newcachedentry.url = lruvalue.url;
			newcachedentry.lastmodifieddate = lruvalue.lastmodifieddate;
			newcachedentry.expireddate = lruvalue.expireddate;
			newcachedentry.filename = lruvalue.filename;
			lruentries.erase(lruiterator);
			lruentries.push_front(newcachedentry);
		}
		else
		{
			cout<<"\n  Not in cache. So adding entry to cache.\n";
			//Cache limit reached.Pop the oldest entry and delete its file.
			if(lruentries.size() == NUMBER_OF_CACHED_ENTRIES)
			{
				newiterator = lruentries.begin();
				for(int j=0;j < (int)lruentries.size();newiterator++,j++)
				lruvalue = *newiterator;
				if(remove(lruvalue.filename.c_str()) != 0)
					cout<<"\n Error deleting file "<<lruvalue.filename;
				else
					cout<<"\n The file "<<lruvalue.filename<<" is successfully deleted.";

				lruentries.pop_back();
			}
				newcachedentry.url = receivedurl+"/"+relativepath;
				//cout<<"\n receivedurl "<<receivedurl<<"\n newcachedentry.url "<<newcachedentry.url;
				dateinfo = callhead(receivedurl,relativepath);
				newcachedentry.lastmodifieddate = dateinfo[0];
				newcachedentry.expireddate = dateinfo[1];
				if(newcachedentry.lastmodifieddate == FILE_NOT_FOUND)
				{
					cout<<"404 File not found";
					return FILE_NOT_FOUND;
				}		
				newcachedentry.filename = callget(receivedurl,relativepath);
				//Add new entry to Cache at the front
				lruentries.push_front(newcachedentry);
		}
	}

	cout<<"\n Printing cache from Most Recent First\n";
	cout<<" ----------------------------------------\n";
	int temp_number = 1;
	for(newiterator = lruentries.begin();newiterator != lruentries.end();newiterator++)
	{
		printvalue = *newiterator;
		cout<<"\n"<<temp_number<<"  "<<printvalue.url<<"\t"<<printvalue.filename;
		temp_number++;
	}

	return newcachedentry.filename;

}

//Call HEAD request to get details like Last modified date,expires date. Returns these 2 values.
std::string* callhead(std::string receivedurl,std::string relativepath)
{
	std::string headcommand;
	std::string headdata;
	std::string lastmodificationpattern = "Last-Modified:";
	std::string expirespattern = "Expires:";
	std::string successresponse = "200 OK";
	std::string *dateinfo = new string[2];
	char buffer[100000];
	int searchresult,sendresult,receiveresult,httpserversd;
	headcommand = "HEAD /";
    headcommand = headcommand + relativepath;
    headcommand = headcommand + " HTTP/1.0\n\n";
	
	httpserversd = createhttpsocket(receivedurl);
	
	cout<<"\n Head command is "<<headcommand;
	//fptr = popen(headcommand.c_str(),"r");
	//Send HEAD to HTTP server
	sendresult = send(httpserversd,headcommand.c_str(),headcommand.length(),0);

	if(sendresult <= 0)
	{
		cout<<"\n Error sending to HTTP server.";
	}
	//Receive from HTTP Server
	receiveresult = recv(httpserversd,buffer,100000,0);

	//cout<<"\n Received Buf is \n"<<buffer<<endl;
	
	headdata = buffer;
   		   
		
//On unsuccessful find, find functions returns a value string::npos. So checking for successful search result.
	if(headdata.find(successresponse) != string::npos)
	{
		searchresult = headdata.find(lastmodificationpattern,0);

		if(searchresult != (int)string::npos)
		{	
			for(int i=searchresult+lastmodificationpattern.length();headdata.at(i) != '\n';i++)
				dateinfo[0] += headdata.at(i);//Store last modified date to a variable
			cout<<"\n Last modified date is "<<dateinfo[0];
		}
		else{
			dateinfo[0] = NO_TIMESTAMP;
		}
	}
	else
	{
		dateinfo[0] = FILE_NOT_FOUND;
	
	}

//On unsuccessful find, find functions returns a value string::npos. So checking for successful search result.	
	searchresult = headdata.find(expirespattern,0);
	if(searchresult != (int)string::npos)
	{
		for(int i=searchresult+expirespattern.length();headdata.at(i) != '\n';i++)
				dateinfo[1] += headdata.at(i);
			cout<<"\n Expired date is "<<dateinfo[1];
	
	
	}
	else
	{
		dateinfo[1] = NO_TIMESTAMP;
	
	}
	close(httpserversd);
	return dateinfo;
}

//Call GET request to get web page. Returns the filename it stored the web page
std::string callget(std::string receivedurl,std::string relativepath)
{
	std::string getcommand;
	std::string getdata;
	std::string filename;
	FILE *serverfile;
	int sendresult,receiveresult,httpserversd;
	char sendbuf[100000];
	char buffer[100000];	
	char fname[BUFFER_SIZE];

	strcpy(sendbuf,"GET /");
	strcat(sendbuf,relativepath.c_str());
	strcat(sendbuf," HTTP/1.0\n\n");
	
	httpserversd = createhttpsocket(receivedurl);

	//cout<<"\n Get command:"<<sendbuf;
	//Name of file being used to store web page
	sprintf(fname,"client%d",nooffiles);
	filename = fname;
	nooffiles++;

	cout<<"\nFile written to is "<<filename;

	serverfile = fopen(fname,"w");
	//Send GET to HTTP Server
	sendresult = send(httpserversd,sendbuf,sizeof(sendbuf),0);

	if(sendresult <= 0)
	{
		cout<<"\n Error sending to HTTP server.";
	}
	bzero(buffer,100000);
	//Receive from HTTP server
	receiveresult = recv(httpserversd,buffer,100000,0);

	//cout<<"\n Received Buf is \n"<<buffer<<endl;
	
	getdata = buffer;
	
	  fputs(buffer,serverfile);
	fclose(serverfile);
	close(httpserversd);
			
	return filename;	

}

//Send the file to HTTP client
void sendtoclient(std::string filename,int sd)
{
	char sendbuf[100000];
	FILE *pFile;
	int readresult,sendresult;
	//Open the file to send
	pFile = fopen(filename.c_str(),"r");


	if(pFile)
	{
		strcpy(sendbuf,filename.c_str());
		//cout<<"\n Send buffer "<<sendbuf;
		send(sd,sendbuf,strlen(sendbuf),0);//Send filename 1st so that cleint can create a file with this name and later copy data.
		bzero(sendbuf,100000);
		while(!feof(pFile))
		{
			readresult = fread(sendbuf,1,100000,pFile); 
	//Send to HTTP client
			sendresult = send(sd,sendbuf,strlen(sendbuf),0);

			 if(sendresult < 0)
			  {
				cout<<"\n Error while sending";
				break;
			  }

			  bzero(sendbuf,100000);			  
		}
	}
	else
	{
		cout<<"\n Error opening file.";
	}
	  
	  cout<<"\n File sent!";
	
	  fclose(pFile);
}

//Validate the incoming packet from Client
std::string validatepacket(char *receivebuf)
{
	std::string httprequest,url;
	//std::string::iterator it;
	int pos;
	httprequest = receivebuf;

	pos = httprequest.find("GET / HTTP/1.0");
	//std::advance (it,pos+15);
	if(pos != (int)string::npos)
	{
		url = httprequest.substr(pos+15);
	}
	else
	{
		url = INCORRECT_HTTP_REQUEST;
	}
	cout<<"\n URL is "<<url;

	return url;
}

//Get relative path of the URL. Eg:index.html
std::string getrelativepath(std::string url)
{
  std::string relativepath;
  int pos;
  pos = url.find("/");
  if(pos != (int)string::npos)
    {
      relativepath = url.substr(pos+1);  
    }
  else
  {
    relativepath = "";
  }
  cout<<"\n Relative path is:"<<relativepath;
  return relativepath;
}

//Get the IP address of the URL.
std::string geturlipaddress(std::string url)
{
	int pos;
	std::string urlip;
	pos = url.find("/");
	if(pos != (int)string::npos)
	{		
		  urlip = url.substr(0,pos);
	}
	else
	{
		urlip = url;	
	}
	cout<<"\n IP address is "<<urlip<<":";
	return urlip;

}

//Create a socket to contact HTTP server and return socket id
int createhttpsocket(std::string urlip)
{
						int httpserversd,yes = 1;
						struct sockaddr_in httpserveraddress;
					//Create a socket 
					  httpserversd = socket(AF_INET,SOCK_STREAM,0);

					//Check for any error in socket creation
					  if(httpserversd == -1)
					    {
					       cout<<"\n Unable to create socket";
					       exit(1);
					    }

					  //Reuse the socket
					  setsockopt(httpserversd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

					  //Initialize value to 0
					  //memset(&httpserveraddress,0,sizeof(httpserveraddress));
					  bzero(&httpserveraddress,sizeof(httpserveraddress));
					  httpserveraddress.sin_family = AF_INET;
					  httpserveraddress.sin_port = htons(80);//host to network order conversion
					  httpserveraddress.sin_addr.s_addr = inet_addr(urlip.c_str());					 
					 

					//Establish connection to server0
					 if(connect(httpserversd,(struct sockaddr *)&httpserveraddress,sizeof(httpserveraddress)) == -1)
					  {
					    cout<<"\nCannot connect to HTTP server\n";
					    return -1;
					    //exit(1);
					  }
					  
					  cout<<"\n Connected to HTTP server";
					  
					  return httpserversd;

}

//Check if Current date and time is greater than Expired date and time.Returns 1 if current time < Expired time.0 if current time > Expired time.
int fromcacheornot(std::string datestr)
{
	//To find the month in number format we search for these patterns.
	char months[][12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	int pos,month,daypos,dayofmon,yr,hour,min,sec;
	std::string day,year,timeofday;
	time_t rawtime;//THIS GIVES THE CURRENT TIME IN GMT FORMAT
    struct tm * ptm;//time.h

  time(&rawtime);
  ptm = gmtime(&rawtime);//struct to a pointer having time parameters
	for(int i=0;i<12;i++)
	{
		//Find position of month filed like Jan,Feb,etc.
		pos = datestr.find(months[i]);
		if(pos != (int)string::npos)
		{
			month = i+1;
			break;
		}		
	}
	daypos = datestr.find(",");//Find day of month
	
	if(datestr.at(daypos+3) != ' ')
	{
		day = datestr.substr(daypos+2,2);//Handling single and double digits
	}	
	else
	{
		day = datestr.at(daypos+2);
	}
		
	dayofmon = atoi(day.c_str());//Change to int format
	year = datestr.substr(pos+4,4);	//Get year of expiry
	yr = atoi(year.c_str());			
		
		//time.h returns no of years from 1900
	if((1900+ptm->tm_year) 	> yr)
		return 0;
	else
	{
		//time.h returns month number whose idnex starts from 0 for Jan.
		if((ptm->tm_mon+1) > month)
			return 0;
		else
			{
				//Day of month
				if(ptm->tm_mday > dayofmon)
					return 0;
				else
					{
						//Get time of the day of expiry from Message
						timeofday = datestr.substr(pos+9,8);
						//cout<<"\n Time of expiry "<<timeofday;
						hour = atoi(timeofday.substr(0,2).c_str());
						//cout<<"\n Hour of expiry "<<hour;
						min = atoi(timeofday.substr(3,2).c_str());
						//cout<<"\n Min of expiry "<<min;
						sec = atoi(timeofday.substr(6,2).c_str());
						//cout<<"\n Sec of expiry "<<sec;
						time_t rawtime;
						struct tm * ptm;

						time(&rawtime);
						ptm = gmtime(&rawtime);
						  
						  if(ptm->tm_hour > hour)
							return 0;
						  else
						  {
							if(ptm->tm_min > min)
							  return 0;
							else
								{
								  if(ptm->tm_sec > sec)
								    return 0;
								  else
									return 1;
								}
						  
						  }
						  
					}
			}
			
	}		
			


}
