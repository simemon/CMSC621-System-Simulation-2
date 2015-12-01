#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <arpa/inet.h>

#define MESSAGE_LENGTH 256
#define MAX_SENSOR 20
#define DEFAULT_INTERVAL 5
#define NUMBER_OF_DEVICES 3
#define MAX_CONNECTION 20

struct 
{
	int sockid;
	int interval;
}CurrInterval[MAX_SENSOR];

typedef struct 
{
	char IP[16], Port[7] , type[20];
	int sockid;
  int counter;
}sens_dev;

int caller = 0;

pthread_t tid;
int clnt;
char GPort[7], GIP[30],SensPort[7],SensArea[20],SensIP[16]; 
int SensorCount = 0;
sens_dev connList[MAX_CONNECTION];
int connCount = 0;
int selfcounter = 0;
int my_vector[NUMBER_OF_DEVICES] = {0,0,0};
int my_index = -1;;
char *outFile;
FILE *sensorFile;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


void* InitParams(void*);

/*
int extract_val(char *msg, int index)
{
  int temp_arr[3];
  char temp_str[100];
  printf("Msg in extract_val: %s\n", msg);
  sscanf(msg,"%s#%d:%d:%d",temp_str,&temp_arr[0],&temp_arr[1],&temp_arr[2]);
  printf("Remaining message: %s\n", temp_str);
  printf("temp_arr: %d %d %d\n",  temp_arr[0], temp_arr[1], temp_arr[2]);
  printf("Value in extract_val: %d\n", temp_arr[index]);
  return temp_arr[index];
}
*/


void printVector()
{
  int i;
  printf("Vector: ");
  for (i = 0; i < 3; ++i)
  {
    printf("%d ", my_vector[i]);
  }
  printf("\n");
}

void printArray()
{
	int i;
  //printf("ARRAY PRINTING\n");
	for (i = 0; i < 2; ++i)
	{
		//printf("connList[%d]\n", i );
		//printf("Sockid: %d, IP: %s, Port: %s, Type: %s\n", connList[i].sockid, connList[i].IP, connList[i].Port, connList[i].type );
	}
}

void InitConfiguration(char *filename)
{
	FILE *cfg;
	cfg = fopen(filename,"r");
	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	//Tokenize configuration with : separator
	fscanf(cfg,"%[^:]:%s\nsensor:%[^:]:%[^:]:%s",GIP,GPort,SensIP,SensPort,SensArea);

  if(!strcmp(SensArea,"Door"))
    caller = 1;
  else if(!strcmp(SensArea,"MotionDetector"))
    caller = 2;
  else if(!strcmp(SensArea,"KeyChain"))
    caller = 3;

  //printf("caller: %d\n", caller);

	fclose(cfg);
}

int max(int n1, int n2)
{
  if(n1 > n2)
    return n1;
  return n2;
}

void updateVectors(char *buffer)
{
  char temp_type[20];
  char temp_device[20];
  char temp_value[10];
  int temp_vector[3];
  int i;

  //printf("In updateVectors\n");

  sscanf(buffer,"Type:%[^;];Device:%[^;];Value:%[^#]#%d:%d:%d",temp_type,temp_device,temp_value,&temp_vector[0],&temp_vector[1],&temp_vector[2]);
  
  //printf("Old Vector\n");
  //printVector();

  for(i=0;i<3;i++)
  {
    //printf("Vector updated\n");
    my_vector[i] = max(my_vector[i],temp_vector[i]);
  }
  //printVector();
}

void selection3(char *fname)
{
	int newsockfd[2], portno, n;
  int i;
   struct sockaddr_in serv_addr;
   struct hostent *server;
   
   char buffer[256];
  
  //printf("Entered in Selection 3\n");

   for(i = 0 ;i < 2 ;i++)
   {
      /* Create a socket point */
     newsockfd[i] = socket(AF_INET, SOCK_STREAM, 0);
     
     if (newsockfd[i] < 0) {
        perror("ERROR opening socket");
        exit(1);
     }
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = inet_addr(connList[i].IP);
     serv_addr.sin_port = htons(atoi(connList[i].Port));

     printArray();

     /* Now connect to the server */
     if (connect(newsockfd[i], (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connect failed\n");
        printf("ERROR connecting: %s\n",strerror(errno));
        exit(1);
     }

     //printf("Type[%d]: %s\n", i, SensArea );
     //SEND A MESSAGE ABOUT TYPE
     write(newsockfd[i], SensArea, sizeof(SensArea));
     connList[i].sockid = newsockfd[i];  
   }
	
   printArray();

  //printf("Thread creation\n");
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
    exit(1);
  }

   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
  while(1)
  {
    rfds = alternative;
    //printf("Before select\n");
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    //printf("After Select\n");

    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      /*printf("Greater than 1\n");*/
      for(ind=0;ind<2;ind++)
      {
        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[ind], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        //printf("newsockfd0\n");
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n",newsockfd[0], buffer);
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);

      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
        //printf("newsockfd1\n");
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[1], buffer);
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }
   
}

void selection2(char *fname)
{
	int master_sockfd, newsockfd[2], portno, clilen;
   char buffer[256];
   struct sockaddr_in serv_addr, cli_addr;
   int  n,i, temp_index;
   char message[MESSAGE_LENGTH];
   int message_size;
   
   /* First call to socket() function */
   master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (master_sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
   }
   
   /* Initialize socket structure */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   portno = atoi(SensPort);
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);
   
   /* Now bind the host address using bind() call.*/
   if (bind(master_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
   }
      
   /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
   
   listen(master_sockfd,5);
   clilen = sizeof(cli_addr);
   
   /* Accept actual connection from the client */
   newsockfd[0] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);
   //newsockfd1 = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	
  //printf("After accept\n");
  
  /*
   read call to get the information about the device which is going to sent the information
   */

   bzero(message,MESSAGE_LENGTH);

   //printf("Waiting for read\n");
	if((message_size = read(newsockfd[0],message,256))<0)
	{
		perror("Received Message Failed");
		exit(1);
	}
	printf("Read: -- %s\n", message  );
	
	for(i=0; i< 2; i++)
	{
    //printf("Before comparison\n");
		if(strcmp(message,connList[i].type) == 0)
		{
			connList[i].sockid = newsockfd[0];
			break;
		}
	}
  /* Create a socket point */
   newsockfd[1] = socket(AF_INET, SOCK_STREAM, 0);
   
   if(i == 0)
   	temp_index = 1;
   else
   	temp_index = 0;

   //printf("temp_index: %d\n", temp_index );

	connList[temp_index].sockid = newsockfd[1];

   printArray();

   if (newsockfd[1] < 0) {
      perror("ERROR opening socket");
      exit(1);
   }

   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = inet_addr(connList[temp_index].IP);
   serv_addr.sin_port = htons(atoi(connList[temp_index].Port));

   /* Now connect to the server */
   if (connect(newsockfd[1], (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR connecting");
      exit(1);
   }

    //printf("Type[%d]: %s\n", i, SensArea );
    //SEND A MESSAGE ABOUT TYPE
    write(newsockfd[1], SensArea, sizeof(SensArea));

    //printf("After accept and connect\n");
    printArray();
   
   if (newsockfd[0] < 0) {
      perror("ERROR on accept");
      exit(1);
   }

   if (newsockfd[1] < 0) {
      perror("ERROR on accept");
      exit(1);
   }
   
   /* If connection is established then start communicating */
   //bzero(buffer,256);
  
  //printf("Thread creation\n"); 
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
    exit(1);
  }

   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
  while(1)
  {
    rfds = alternative;
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    
    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      for(ind=0;ind<2;ind++)
      {
        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[ind], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }
        printf("Received from %d: %s\n", newsockfd[0], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[1], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }
}

/* 2 accept - server */
void selection1(char* fname)
{

   int newsockfd[2], portno, clilen;
   char buffer[256];
   struct sockaddr_in serv_addr, cli_addr;
   int  n, i , j;
   int master_port = atoi(SensPort);
   int master_sockfd = -1;
   char message[MESSAGE_LENGTH];
   int message_size;

   //printf("Filename : %s\n", fname );

   /* First call to socket() function */
   master_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (master_sockfd < 0) {
      perror("ERROR opening socket");
      exit(1);
   }
   
   /* Initialize socket structure */
   bzero((char *) &serv_addr, sizeof(serv_addr));
   //portno = atoi(argv[1]);
   
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(master_port);
   
   /* Now bind the host address using bind() call.*/
   if (bind(master_sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      perror("ERROR on binding");
      exit(1);
   }
      
   /* Now start listening for the clients, here process will
      * go in sleep mode and will wait for the incoming connection
   */
   
   listen(master_sockfd,5);
   clilen = sizeof(cli_addr);
   
   /* Accept actual connection from the client */
   newsockfd[0] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);
   newsockfd[1] = accept(master_sockfd, (struct sockaddr *)&cli_addr, &clilen);

   if (newsockfd[0] < 0) {
      perror("ERROR on accept");
      exit(1);
   }

   if (newsockfd[1] < 0) {
      perror("ERROR on accept");
      exit(1);
   }
   
   //printf("Accept done successful\n");

   for(i=0;i<2;i++)
   {
    
    bzero(message,MESSAGE_LENGTH);

    if((message_size = read(newsockfd[i],message,256))<0)
    {
      perror("Received Message Failed");
      //return 0;
    }

    if(strstr(message,"exit"))
    {
      printf("Exit Message\n");
      exit(0);
    }

    /*
    compare with all the connection type and link socket to that particular device 
    so that we can use those sockfd later.

    */
    for(j=0;j<2;j++)
    {
      if(strcmp(connList[j].type,message) == 0)
      {
      	//printf("Both are same : %s\n", connList[j].type );
      	connList[j].sockid  = newsockfd[i];

      	break;
      }
    }

  }
  printArray();
  //printf("Thread Created\n");
 
  if(pthread_create(&tid,NULL,InitParams,(void*)fname) != 0)
  {
    perror("Thread creation failed");
  }

   /* If connection is established then start communicating */
   //bzero(buffer,256);
   char c;
   fd_set rfds, alternative;
   struct timeval tv;
   int retval,ind;

   /* Watch stdin (fd 0) to see when it has input. */
   FD_ZERO(&alternative);
    //FD_ZERO(&rfds);
   FD_SET(newsockfd[0],&alternative);
   FD_SET(newsockfd[1],&alternative);
  
   
  while(1)
  {
    rfds = alternative;
    //printf("Before select\n");
    retval = select(FD_SETSIZE, &rfds, NULL, NULL, NULL);
    
    if(retval == -1)
    {
      printf("Select Error\n");
      exit(1);
    }
    else if(retval > 1)
    {
      //printf("Greater than 1\n");
      for(ind=0;ind<2;ind++)
      {

        bzero(buffer,256);
        read(newsockfd[ind],buffer,sizeof(buffer));

        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[ind], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
    else if(retval == 1)
    {
      if(FD_ISSET(newsockfd[0],&rfds))
      {
        //printf("newsockfd[0]\n");
        bzero(buffer,256);
        read(newsockfd[0],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[0], buffer);
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
      else if(FD_ISSET(newsockfd[1],&rfds))
      {
        //printf("newsockfd1\n");
        bzero(buffer,256);
        read(newsockfd[1],buffer,sizeof(buffer));
        
        if(strstr(buffer,"exit"))
        {
          printf("Exit Message\n");
          exit(0);
        }

        printf("Received from %d: %s\n", newsockfd[1], buffer );
        
        pthread_mutex_lock(&mutex);
        updateVectors(buffer);
        pthread_mutex_unlock(&mutex);
      }
    }
  }

}

void* InitParams(void *filename)
{
	FILE *file;
  char msg[100];
	int start_time;	
	char val[20];
  int current_time = 0, end_time = -1;	//to set 0 to check initial condition
	char temp_str[20];
	int tmp_time = 1, i;
	char *fname = (char *) filename;
  bool flag_remain = false;
  bool door_file_end = false;

  //printf("outFile: %s\n", outFile);

  file = fopen(fname,"r");	

  if(!file)
	{
		printf("Sensor Input not found\n");
		exit(1);
	}		

  while(true)
	{	
    if(!strcmp(SensArea, "Door"))
    {
      my_index = 0;
  		fscanf(file,"%d;%s\n",&end_time,val);

  		sleep(end_time-current_time);

      if(feof(file))
  		{
        //printf("End  of File\n");
        door_file_end = true;
        current_time = 0;
				fseek(file, 0, SEEK_SET);
  		}
      else
      {
        current_time = end_time;
      }
    }
		else 
    {
      if(!strcmp(SensArea, "MotionDetector"))
      {
        //printf("MotionDetector\n");
        my_index = 1;
      }
      else
      {
        my_index = 2;
      }

      //printf("Before flag_remain condition: %d\n", flag_remain );
      if(!flag_remain)
      {
        fscanf(file,"%d,%d,%s\n",&start_time,&end_time,val);
        //printf("start_time: %d, end_time: %d, current_time: %d\n", start_time,end_time,current_time);
        
        if (current_time == 0 && start_time != 0)
        {
          /*printf("Sleep due to start_time\n");*/
          sleep(start_time);
          current_time = start_time;
        }
      }
    } //end for motion and key sennsor
    
    pthread_mutex_lock(&mutex);
    my_vector[my_index] = my_vector[my_index] + 1;
    printVector();
    pthread_mutex_unlock(&mutex);

    //Msg sent to Gateway
    bzero(msg,100);

    pthread_mutex_lock(&mutex);
    sprintf(msg, "Type:currValue;Device:%s;Time:%d;Value:%s#%d:%d:%d",SensArea,(int)time(NULL),val,my_vector[0],my_vector[1],my_vector[2]);
    
    printf("Message Sent to Gateway: %s\n",  msg);
		
    if(send(clnt,msg,strlen(msg),0) < 0)
		{
			perror("Message Sent Failed");
		}
    //Msg sent to other devices
    bzero(msg,100);
    sprintf(msg, "Type:currValue;Device:%s;Value:%s#%d:%d:%d",SensArea,val,my_vector[0],my_vector[1],my_vector[2]);
    pthread_mutex_unlock(&mutex);
    
    for(i =0 ;i < connCount; i++)
    {
      if(send(connList[i].sockid,msg,strlen(msg),0) < 0)
      {
        perror("Message Sent Failed");
      }
      //printf("Message sent to %d as %s\n", connList[i].sockid, msg);
    }

    /* Need to store this msg into Output File  */
    fprintf(sensorFile, "Sent: %s\n", msg);
    fflush(sensorFile);

    if(my_index != 0)
    {
      //printf("Before sleep\n");
      sleep(5); 
      current_time += 5;
      //printf("Increased current_time: %d\n", current_time);

      if(current_time <= end_time)
          flag_remain = true;

      else if(feof(file))
      {
        //printf("End of File happened\n");
        fseek(file, 0, SEEK_SET);
        current_time = 0;
        flag_remain = false;
      }
      else
      {
        flag_remain = false;
      }
    }
    else
    {
      if(door_file_end)
      {
        sleep(1);
        door_file_end = false;
      }
    }
	}
}

int TryConnection()
{
	struct sockaddr_in sock;
	int clnt;	//Client FD
	clnt = socket(AF_INET,SOCK_STREAM,0);
	if(clnt < 0)	//Socket Connetion Failed
	{
		perror("Socket Create Failed");
		exit(0);
	}
	
	sock.sin_family = AF_INET;
	sock.sin_addr.s_addr = inet_addr(GIP);
	sock.sin_port = htons(atoi(GPort));
	
	if(connect(clnt, (struct sockaddr*)&sock, sizeof(sock)) < 0)
	{	
		perror("Connetion Failed");
		close(clnt);
		exit(-1);
	}
	

	/* If Connection is successful, send connected  socket Failed */
	return clnt;
}

void registerDevice(int clnt, char* fname)
{

	char msg[MESSAGE_LENGTH];
	int i;
	char temp_IP[20];
	char temp_Port[7];
	char temp_type[20];
	int message_size;

	CurrInterval[SensorCount].sockid = clnt;
	CurrInterval[SensorCount].interval = DEFAULT_INTERVAL;

	SensorCount += 1;

	sprintf(msg,"Type:register;Action:%s-%s-%s",SensIP,SensPort,SensArea);

  //printf("Message Sent: %s\n", msg);

	if(send(clnt,msg,strlen(msg),0)<0)
	{
		perror("Message Sent Failed");
	}
	//sleep(1);

	// UMBC

	/*
	1. Recv message from gateway
	2. Process it and save in structure array
	3. create a thread and use a function
	4. In a function, parsed that message and save it global array
	5. compare socket details with config file and maintain my pointer (which addr refers to me)
	6. use server.c to accept two connectins and perform select
	*/
  for(i=0;i<NUMBER_OF_DEVICES;i++)
	{
		bzero(msg,MESSAGE_LENGTH);

		//printf("Before read\n");
		
		if((message_size = read(clnt,msg,256))<0)
		{
			perror("Received Message Failed");
			//return 0;
		}
		//printf("Message: %s\n", msg);

		sscanf(msg,"%[^-]-%[^-]-%s",temp_type,temp_IP,temp_Port);
		if(strcmp(SensPort,temp_Port) == 0 && strcmp(SensIP,temp_IP) == 0)
		{
			//printf("My address info: %s-%s\n", temp_IP, temp_Port );
	    continue;
		}
		strcpy(connList[connCount].type,temp_type);
		strcpy(connList[connCount].IP,temp_IP);
		strcpy(connList[connCount].Port,temp_Port);

		//printf("%s %s %s\n", connList[i].IP,connList[i].Port,connList[i].type);
    connCount++;
	}

  //printf("BEFORE selection CALL\n");
  //printf("ConnCount : %d\n", connCount );

  printArray();

  if(caller == 1)
	 selection1(fname);
  else if(caller == 2)
    selection2(fname);
  else
    selection3(fname);
}

void  KillHandler(int sig)
{
  int i;
  signal(sig, SIG_IGN);
  for(i =0 ;i < connCount; i++)
    {
      if(send(connList[i].sockid,"exit",strlen("exit"),0) < 0)
      {
        perror("Message Sent Failed");
      }
      printf("Exit\n");
    }
  kill(getpid(),SIGKILL);
}

int main(int argc, char *argv[])
{
	
  if(argc < 4)
	{
		printf("Please provide exact number of arguments\n");
		return 0;
	}
  
  //printf("argument count: %d\n", argc);
  
  sensorFile = fopen(argv[3],"w");  
  
  if(!sensorFile)
  {
    printf("Sensor Output not found\n");
    exit(1);
  }

  signal(SIGINT, KillHandler); 
  signal(SIGTSTP, KillHandler);
  
	InitConfiguration(argv[1]);
	clnt = TryConnection();
	registerDevice(clnt, argv[2]);
	return 0;
}

