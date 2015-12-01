#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_CONNECTION 20
#define MESSAGE_LENGTH 256

#define BACKEND_SERVER "localhost"
#define BACKEND_PORT 5678

#define NUMBER_OF_DEVICES 3
#define HANDLER_TIMER 20

char *status_list[] = {"off", "on"};
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_current = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_output = PTHREAD_MUTEX_INITIALIZER;

typedef struct 
{
	char IP[16], Port[7], Area[5];
	char type[15];
	int sockid;
	int id;
}sens_dev;

typedef struct
{
	int sockid;
	struct sockaddr addr;
	int addr_len;
}connection_ds;

int connCount = 0; 
int SensorCount = 0;
FILE *devStatus;
sens_dev connList[MAX_CONNECTION];

char SecurityIP[20],SecurityPort[7];
int SecurityStatus = 0, SecuritySockID = -1;


char GPort[7],GIP[20];


FILE *fp_gateway;

pthread_t thread, thread_setVal;
int sockfd;

connection_ds *CurrClient = NULL;
bool killer = false;


int backend_port = BACKEND_PORT;
int backend_sock = -1;
struct sockaddr_in backend_address;
struct hostent * backend_host;
int backend_len;

bool monitor_key = false;
bool monitor_motion = false;
int motion_detected = 2;	//Initial value (to reset)
int key_detected = 2;	//Initial Value (to reset)

/* Buffering - Link List and Queue 	*/

int current_vector[3];

struct node
{
	int vector[3];
	struct node *next;
	char msg[100];
};

struct node *start = NULL;

void InitCurrentVector()
{
	int i;
	for (i = 0; i < 3; ++i)
	{
		current_vector[i] = 0;
	}
}

void printQueue()
{
	struct node *traverse = start;
	int i;
	//printf("printQueue\n");
	while(traverse)
	{
		//printf("Message: %s\n", traverse->msg);
		for (i = 0; i < 3; ++i)
		{
			//printf("%d ", traverse->vector[i]);
		}
		//printf("\n");
		traverse = traverse->next;
	}
}

void storeinQueue(int v[3],char *msg)
{
	struct node *temp,*traverse;
	int i;
	
	temp = (struct node *) malloc(sizeof(struct node));
	temp->next = NULL;
	strcpy(temp->msg,msg);

	for(i=0;i<3;i++)
	{
		temp -> vector[i] = v[i];	
	}
	if (!start)
	{
		start = temp;
		return;
	}
	traverse = start;
	while(traverse->next != NULL)
		traverse = traverse->next;
	traverse->next = temp;
}

int max(int a, int b)
{
	if(a>b)
		return a;
	return b;
}

bool isQueue()
{
	if(start)
		return true;
	return false;
}

void printVector(int n)
{
	int i,j;

	//printf("Vector Print: \n");
	for (i = 0; i < n; ++i)
	{
		//printf("\n");
		for (j = 0; j < 3; ++j)
		{
			//printf("%d ", v[i][j]);
		}
	}
	//printf("\n");
}

int isExpected(int c[3])
{
	bool flag = false;
	int i;

	for(i=0;i<3;i++)
	{
		if (c[i] > current_vector[i] + 1)
		{
			return 0;
		}
		else if ((c[i] == current_vector[i] + 1) && flag == false)
		{
			flag = true;
		}
		else if ((c[i] == current_vector[i] + 1) && flag == true)
		{
			return 0;
		}
	}
	return true;
}

struct node* RetrieveQueue()
{
	struct node *traverse = NULL;
	int i;
	struct node *temp = NULL, *prev;

	if(!start)
	{
		return NULL;	
	}
	if(isExpected(start->vector))
	{
		//printf("Retrieved Message: %s\n", start->msg);

		temp = start;
		start = start -> next;
		return temp;
	}
	traverse = start->next;
	prev = start;
	while(traverse)
	{
		printf("Message: %s\n", traverse->msg);

		if(isExpected(traverse->vector))
		{
			prev -> next = traverse -> next;
			return traverse;	
		}
		prev = traverse;
		traverse = traverse->next;
	}	
	return NULL;
}

void updateCurrent(int v[3])
{
	int j;
	for (j = 0; j < 3; ++j)
	{
		current_vector[j] = max(current_vector[j],v[j]);
	}
	printf("\n");
}

/* End of Link List and Buffering Code */



int findConnection(int sockid)
{
	int i;
	for(i=0;i<connCount;i++)
	{
		if(connList[i].sockid == sockid)
			return i;
	}
	return -1;
}

void PrintConnection()
{
	int  i;
	for (i = 0; i <= connCount; ++i)
	{
		printf("ID: %d, SockID: %d, IP: %s, Port: %s, Type: %s\n", connList[i].id,connList[i].sockid,connList[i].IP,connList[i].Port,connList[i].type);
	}
	printf("\n");
}

void RegisterWithBackend(char *host, char *p)
{
		/* create socket */
	backend_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (backend_sock <= 0)
	{
		fprintf(stderr, "error: cannot create socket\n");
		exit(1);
	}
		/* connect to server */
	backend_port = atoi(p);
	backend_address.sin_family = AF_INET;
	backend_address.sin_port = htons(backend_port);
	backend_host = gethostbyname(host);
	
	if (!backend_host)
	{
		fprintf(stderr, "error: unknown host \n");
		exit(1);
	}

	memcpy(&backend_address.sin_addr, backend_host->h_addr_list[0], backend_host->h_length);
	if (connect(backend_sock, (struct sockaddr *)&backend_address, sizeof(backend_address)))
	{
		fprintf(stderr, "error: cannot connect to host \n");
		exit(1);
	}

	/* send text to server */
	backend_len = 8;	/* length of Register word*/
	write(backend_sock, &backend_len, sizeof(int));
	write(backend_sock, "Register", backend_len);
}

void WriteToBackend(char *str)
{
	/* send text to server */
	//printf("Message  from WriteToBackend: %s\n", str);
	backend_len = strlen(str);
	write(backend_sock, &backend_len, sizeof(int));
	write(backend_sock, str, backend_len);	
}

void ResetMonitor()
{
	//Reseting initial conditions
	monitor_motion = false;
	monitor_key = false;
	motion_detected = 2;
	key_detected = 2;
}

int UserDetection(char *device, char *value)
{
	if(!(strcmp(device,"Door")) && (!strcmp(value,"Open")))
	{
		ResetMonitor();		
		return 2;
	}
	else if(!(strcmp(device,"Door")) && (!strcmp(value,"Close")))
	{
		monitor_motion = true;
		monitor_key = true;
		printf("Monitoring started\n");
		return 2;
	}

	else if(monitor_motion && monitor_key)
	{
		if(!(strcmp(device,"MotionDetector")) && (!strcmp(value,"True")))
		{
			motion_detected = 1;
		}
		else if(!(strcmp(device,"MotionDetector")) && (!strcmp(value,"False")))
		{
			printf("User Exited\n");
			motion_detected = 0;
			return 1;
		}
		else if(!(strcmp(device,"KeyChain")) && (!strcmp(value,"True")))
		{
			key_detected = 1;
		}	
		else if(!(strcmp(device,"KeyChain")) && (!strcmp(value,"False")))
		{
			key_detected = 0;
		}
		if(motion_detected != 2 && key_detected != 2)
		{
			if(motion_detected)
			{
				if(key_detected)
				{
					printf("User Entered\n");
					return 0;	//turn off security system
				}
				else
				{
					printf("Intruder Entered\n");
					return 1;	//turn on security system
				}
			}
			else
			{
				//printf("No one in the house\n");
			}
			ResetMonitor();
		}
	}
	return 2;
}

void SecurityHandler(char *msg)
{
	char temp[5];
	sscanf(msg,"Security-%[^-]-%s",SecurityIP,SecurityPort);
	/*printf("SecurityIP: %s\n", SecurityIP);
	printf("SecurityPort: %s\n", SecurityPort);
	*/
	while(true)
	{
		sleep(HANDLER_TIMER);
	}
}

void * connection(void *clnt)
{
	int client = *(int*)clnt;
		
	int message_size;
	char message[MESSAGE_LENGTH];
	char status[4];
	char message_type[10];
	char action[100];
	char type_temp[20];
	char devmsg[100];
	
	int i,j,ind;
	char temp_area[3];
	bool flag_to_on = false;
	char value[10], device[10];
	int temp_vector[3];
	int msgtime;

	char strToBackend[100];

	int expect = 0;
	struct node *traverse;

	strcpy(devmsg,"");

	while(true)
	{
		if(killer)
			break;

		bzero(message,MESSAGE_LENGTH);

		if((message_size = read(client,message,256))<0)
		{
			perror("Received Message Failed");
			return 0;
		}

		if(message_size != 0)
		{
			//memset(action,0,100);
			
			pthread_mutex_lock(&mutex_output);
			fprintf(fp_gateway, "Received: %s\n", message );
			fflush(fp_gateway);
			pthread_mutex_unlock(&mutex_output);

			bzero(action,100);
			
			if(strstr(message,"register") != NULL)
			{
				
				sscanf(message, "Type:%[^;];Action:%s", message_type,action);
								
				if(strstr(message,"Security") != NULL)
				{
					//printf("SecurityHandler called\n");
					SecuritySockID = client;
					SecurityHandler(action);
				}
			}
			else
			{
				bzero(device,10);
				bzero(value,10);
				sscanf(message, "Type:%[^;];Device:%[^;];Time:%d;Value:%[^#]#%d:%d:%d", message_type,device,&msgtime,value,&temp_vector[0],&temp_vector[1],&temp_vector[2]);
			}
			
			if(strcmp("currValue", message_type) == 0)	//to check type of a msg 
			{

				sscanf(action, "Device:%[^;];Value:%s", device,value);
				/*printf("Value: %s\n", value);
				//printf("TypeTemp: %s\n", type_temp);
				printf("Device: %s\n", device);*/
				int index = findConnection(client);
				
				sprintf(strToBackend,"%d----%s----%s----%d----%s----%s\n",connList[index].id,connList[index].type,value,msgtime,connList[index].IP,connList[index].Port);
	
				/*Adding Buffering part in Gateway Code */

				pthread_mutex_lock(&mutex_current);
				expect = isExpected(temp_vector);

				printf("Device: %s, Value: %s\n", device, value);

				if(expect)
				{
					printf("Expected Vector: ");
				
					for(i = 0; i < 3;i++)
						printf("%d ", temp_vector[i]);
					printf("\n");

					pthread_mutex_lock(&mutex);
					WriteToBackend(strToBackend);
					updateCurrent(temp_vector);
					
					SecurityStatus = UserDetection(device,value);
					
					if (SecurityStatus != 2)
					{
						
						if (SecurityStatus == 1)
							strcpy(value,"On");
						else
							strcpy(value,"Off");

						sprintf(strToBackend,"4----SecuritySystem----%s----%d----%s----%s\n",value,msgtime,SecurityIP,SecurityPort);
						write(SecuritySockID,strToBackend,sizeof(strToBackend));
						WriteToBackend(strToBackend);
					}

					if(isQueue())
					{
						while((traverse = RetrieveQueue()) != NULL)
						{
							WriteToBackend(strToBackend);		
							updateCurrent(traverse->vector);
							
							printf("Retrieved Vector: ");
							for(i = 0; i < 3;i++)
								printf("%d ", traverse->vector[i]);
							printf("\n");

							SecurityStatus = UserDetection(device,value);
							if (SecurityStatus != 2)
							{
								if (SecurityStatus == 1)
									strcpy(value,"On");
								else
									strcpy(value,"Off");

								sprintf(strToBackend,"4----SecuritySystem----%s----%d----%s----%s\n",value,msgtime,SecurityIP,SecurityPort);
								write(SecuritySockID,strToBackend,sizeof(strToBackend));
								WriteToBackend(strToBackend);
							}
						}
					}
					pthread_mutex_unlock(&mutex);
				}
				else
				{
					printf("Unexpected Vector: ");
					for(i = 0; i < 3;i++)
						printf("%d ", temp_vector[i]);
					printf("\n");

					storeinQueue(temp_vector,strToBackend);
				}
				pthread_mutex_unlock(&mutex_current);

				/* End of Link List buffering Code */

				/* security system code to test 
				if (SecurityStatus)
				{	strcpy(value,"On");
				else
					strcpy(value,"Off");

				sprintf(strToBackend,"4----SecuritySystem----%s----%d----%s----%s\n",value,msgtime,SecurityIP,SecurityPort);
				printf("Message: %s\n", strToBackend);
				write(SecuritySockID,strToBackend,sizeof(strToBackend));

				WriteToBackend(strToBackend);
				/* end of security system code to test */

				
			}
			else if(strcmp("register", message_type) == 0)
			{

				//printf("Message from sensor : %s\n", action);
				sscanf(action, "%[^-]-%[^-]-%s", connList[connCount].IP, connList[connCount].Port, connList[connCount].type);
				connList[connCount].sockid = client;
				connList[connCount].id = connCount + 1;
				//printf("In Register Now\n");
				PrintConnection();
				connCount++;

				if(connCount == NUMBER_OF_DEVICES)
				{
					/* Send Message to all the connections to start the communication,
					send information to all the devices so that they can start comm. between
					each other.
					 */
					//printf("Conn Count reached to Number of devices\n");				
										
					for (i = 0; i < connCount; ++i)
					{
						for(j=0;j < connCount;j++)
						{
							sprintf(devmsg,"%s-%s-%s",connList[j].type,connList[j].IP,connList[j].Port);
							//printf("devmsg: %s\n", devmsg);
							write(connList[i].sockid, devmsg, sizeof(devmsg));	
							sleep(1);
							//printf("Write message sent to %d \n", i);
						}
					}
				}
			}
		}
		else
		{
			break;
		}
	}
	
	return 0;
}

void  KillHandler(int sig)
{
  signal(sig, SIG_IGN);
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex);
  kill(getpid(),SIGKILL);
}

void InitConfig(char *fname)
{
	FILE *cfg;
	cfg = fopen(fname,"r");
	if(!cfg)
	{
		printf("Configuration File not found\n");
		exit(1);
	}
	fscanf(cfg,"%[^:]:%s\n",GIP,GPort);
	fclose(cfg);					
}

void TryConnection()
{
	struct sockaddr_in server;
	//initialize Socket
	int userThread = 0;
	int yes=1,clnt=0;
	int temp_sockfd;

	sockfd = socket(AF_INET,SOCK_STREAM,0);
	//Socket Creation

	if(sockfd < 0)
	{
		perror("Socket Creation Failed");
		exit(1);
	}	

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(atoi(GPort));	

	if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes))==-1)
	{
		perror("Socket opt Failed");
		close(sockfd);
		exit(1);
	}

	if(bind(sockfd,(struct sockaddr*)&server,sizeof(server))<0)
	{
		perror("Binding Failed");
		close(sockfd);
		exit(1);
	}
	
	listen(sockfd,3);
	

	printf("Gateway Started Successfully \n");

	CurrClient = (connection_ds*) malloc (sizeof(connection));
	while((CurrClient->sockid = accept(sockfd, &CurrClient->addr, (socklen_t*)&CurrClient->addr_len)))
	{

		temp_sockfd = CurrClient->sockid;
		if((pthread_create(&thread,NULL,connection,(void*)&temp_sockfd))!=0)
		{
			perror("Thread Creation Failed");
		}
		/*
		if(userThread == 0)
		{

			if((pthread_create(&thread_setVal,NULL,setTime,NULL)!=0))
			{
				perror("Thread Creation Failed");
			}

			userThread=1;	
		}
		*/
	}

	if(clnt < 0)
	{
		perror("Accept Failed");
		close(sockfd);
		exit(1);
	}

	fclose(fp_gateway);
	close(sockfd);
				
}


int main(int argc, char *argv[])
{

	if(argc < 4)
	{
		printf("Please provide exact number of arguments\n");
		return 0;
	}

	signal(SIGINT, KillHandler); 
	signal(SIGTSTP, KillHandler);

	fp_gateway = fopen(argv[2],"w");  
  
  	if(!fp_gateway)
  	{
	    printf("Gateway Output not found\n");
	    exit(1);
  	}

	RegisterWithBackend(argv[3],argv[4]);
	sleep(1);
	InitCurrentVector();
	InitConfig(argv[1]);
	TryConnection();
	getchar();
	return 0;
}