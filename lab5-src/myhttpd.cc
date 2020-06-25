
const char * usage =
"                                                               \n"
"daytime-server:                                                \n"
"                                                               \n"
"Simple server program that shows how to use socket calls       \n"
"in the server side.                                            \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   daytime-server <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"In another window type:                                       \n"
"                                                               \n"
"   telnet <host> <port>                                        \n"
"                                                               \n"
"where <host> is the name of the machine where daytime-server  \n"
"is running. <port> is the port number you used when you run   \n"
"daytime-server.                                               \n"
"                                                               \n"
"Then type your name and return. You will get a greeting and   \n"
"the time of the day.                                          \n"
"                                                               \n";


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <malloc.h>
#include <inttypes.h>
#include <string>
#include <vector>
#include <fcntl.h>
#include <semaphore.h>
#include <fstream>
#include <iostream>

int QueueLength = 5;

// Processes time request
void processTimeRequest( int socket );

void processThreadRequest(int socket);
void poolSlave(int socket);
void processCGIBin(int socket, char * root, char * real_path, char * args);

pthread_mutex_t mut;
pthread_mutexattr_t mattr;

int masterSocket;
int slaveSocket;
const char secretkey[] = "";
char * real_path = (char *)malloc(256);

struct Stats {
	struct dirent *dir;
	char *p;
	char *dp;
	struct stat s;
};

extern "C" void killzombie(int sig)
{
	while(waitpid(-1, NULL, WNOHANG) >0);
	close(masterSocket);
}

int
main( int argc, char ** argv )
{
	int port = 0;
	char mode = '\0';
  // Print usage if not enough arguments
  if ( argc < 2 ) {
    fprintf( stderr, "%s", usage );
    exit( -1 );
  }
  // Get the port from the arguments
  //int port = atoi( argv[1] );
	else if(argc == 2)
	{
		port = atoi(argv[1]);
		mode = '\0';
	/*	if(!strncmp(argv[1],"-",1))
		{
			//port = 1087;
			if(argv[1][1] == 'f')
				mode = 'f';
			else if(argv[1][1] == 't')
				mode = 't';
			else if(argv[1][1] == 'p')
				mode = 'p';
			else
				exit(1);
		}*/
	}
	else if(argc == 3)
	{
		if(argv[1][0] == '-')
		{
			if(argv[1][1] == 'f')
				mode = 'f';
			else if(argv[1][1] == 't')
				mode = 't';
			else if(argv[1][1] == 'p')
				mode = 'p';
			else
				exit(1);
		}
		if(atoi(argv[2]) == 0)
			exit(1);
		else
			port = atoi(argv[2]);
	}

	if(port < 1024 || port > 65535)
		exit(1);
 
  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }

  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
   
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
 
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

	printf("before p");

	if(mode != 'p')
	{
		while ( 1 ) {

    			// Accept incoming connections
			    struct sockaddr_in clientIPAddress;
			    int alen = sizeof( clientIPAddress );
			    slaveSocket = accept( masterSocket, (struct sockaddr *)&clientIPAddress,(socklen_t*)&alen);

				printf("after accept");

			    if ( slaveSocket < 0 ) {
			    	perror( "accept" );
      				exit( -1 );
    			    }

				if(mode == '\0')
				{
					if(slaveSocket < 0)
					{
						perror("accept");
						exit(-1);
					}
					printf("slave = %d\n", slaveSocket);
					processTimeRequest(slaveSocket);
					close(slaveSocket);
				}
				else if(mode == 'f')
				{
					pid_t p = fork();
					if(p == 0)
					{
						processTimeRequest(slaveSocket);
						close(slaveSocket);
						exit(1);
					}
					close(slaveSocket);
				}
				else if(mode == 't')
				{
					pthread_t t;
					pthread_attr_t att;
					pthread_attr_init(&att);
					pthread_attr_setscope(&att, PTHREAD_SCOPE_SYSTEM);
					pthread_create(&t, &att, (void *(*)(void *)) processThreadRequest, (void *) slaveSocket);
				}
  		}
	}
	else
	{
		pthread_mutexattr_init(&mattr);
		pthread_mutex_init(&mut, &mattr);
		pthread_t t[QueueLength];
		pthread_attr_t att;
		pthread_attr_init(&att);
		pthread_attr_setscope(&att, PTHREAD_SCOPE_SYSTEM);

		for(int i=0;i<QueueLength;i++)
			pthread_create(&t[i], &att, (void *(*)(void *))poolSlave, (void *)masterSocket);
		pthread_join(t[0], NULL);
	}
	//call killzombie to close mainSocket fd
	struct sigaction sa;
	sa.sa_handler = killzombie;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if(sigaction(SIGINT, &sa, NULL))
	{
		perror("sigaction");
		exit(2);
	}
}

void processThreadRequest(int socket)
{
	processTimeRequest(socket);
	close(socket);
}

void poolSlave(int socket)
{
	while(1)
	{
		pthread_mutex_lock(&mut);
		struct sockaddr_in clientIPAddress;
		int alen = sizeof(clientIPAddress);
		int slaveSocket = accept(socket, (struct sockaddr *)&clientIPAddress, (socklen_t*)&alen);
		pthread_mutex_unlock(&mut);
		processTimeRequest(slaveSocket);
		shutdown(slaveSocket, 1);
		close(slaveSocket);
	}
}

void dirListing(char * path, char * real_path, int fd)
{
	printf("in dirlist\n");
	char * dirListing = NULL;
	dirListing = (char *)malloc(sizeof(char)*1024);
	memset(dirListing, '\0', 1024);
	strcpy(dirListing, "<html>\n\t\t<title>Index of ");
	strcat(dirListing, path);
	strcat(dirListing, "</title>\n\t</head>\n\t<body>\n\t\t<h1>Index of ");
	strcat(dirListing, path);
	strcat(dirListing, "</h1>\n");
	if(strlen(dirListing) >= 896)
		dirListing = (char *)realloc(dirListing, 2048);
	
	printf("before name\n");
	//Name column
	strcat(dirListing, "<table><tr><center><th>Name<href=");
	strcat(dirListing, "?C=N;O=");
	
	strcat(dirListing, "D=");
	strcat(dirListing, "></th><th>Last Modified</th></center></tr><tr><th colspan=\"5\"><hr></th></tr>");
	if(strlen(dirListing) >= 1920)
		dirListing = (char *)realloc(dirListing, 4096);
	DIR * dir = opendir(real_path);
	char d[256] = {0};	
	struct dirent *ent = NULL;
	ent = (struct dirent *)malloc(sizeof(struct dirent*));
	struct stat st;
	printf("before while\n");
	struct dirent **namelist;
	int count = scandir(path,&namelist,NULL,alphasort);
	if(count < 0)
	{
		perror("scandir");
		exit(0);
	}
	bool alpha = true;
	int increment;
	int j;
	if(alpha)
	{
		j = 0;
		increment = 1;
	}
	else
	{
		j = count-1;
		increment = -1;
	}
	for(int i=j;i < count && i >=0;i+=increment)
	{
		ent = namelist[i];
		if(strlen(dirListing) >= strlen(dirListing) * (3/4))
			dirListing = (char *)realloc(dirListing, strlen(dirListing) * 2);
		/*char * rel_path = (char *)malloc(256);
		int i;
		for(i=0;i < strlen(path);i++)
		{
			if(path[i] == '/')
				break;
		}*/
		printf("before path\n");
		
		strcat(dirListing, "<tr><center><td><a href=\"");
		char * cwd = getcwd(d, sizeof(d));
		strcat(cwd, "/http-root-dir");
		strcat(dirListing, &path[strlen(cwd)]);
		if(path[strlen(path)-1] != '/')
			strcat(dirListing, "/");
		//printf("dir dname = %s", &path[46]);
		//strcat(dirListing, ent -> d_name);
		
		//printf("\nent dirname = %s\n", ent->d_name);
		//printf("\ndirlist = %s\n", dirListing);
		char * p = (char *)malloc(256);
		strcpy(p, real_path);
		if(p[strlen(p)-1]!='/')
			strcat(p, "/");
		strcat(p, ent->d_name);
		printf("before stat path = %s\n", p);
		if(stat(p, &st) == -1){
			perror("stat");
			exit(EXIT_FAILURE);
		}
		printf("before name, mtime, size\n");
		strcat(dirListing, "\">");
		strcat(dirListing, "\t");

		strcat(dirListing, ent->d_name);
		strcat(dirListing, "</th><th>");
		printf("%s\n", ctime(&st.st_mtime));
		strcat(dirListing, ctime(&st.st_mtime));
		strcat(dirListing, "</th><th>");
		//strcat(dirListing, (char *)st.st_size);
		//strcat(dirListing, "8");
		//strcat(dirListing, "</th><th>");
		//strcat(dirListing, "this is description");
		strcat(dirListing, "</th></center></tr><tr><th colspan=\"5\"><hr></th></tr>");


		printf("name = %s\n", ent->d_name);
		printf("mod time = %ld\n", (long)st.st_mtime);
		//printf("size = %jd\n", intmax_t(st.st_size));
		//printf("dirlist = %s\n", dirListing);
		strcat(dirListing, "</a></td></center></tr>");
	}
	strcat(dirListing, "<tr><th colspan=\"5\"><hr></th></tr></table></body></html>");

	write(fd, dirListing, strlen(dirListing));
	printf("dirlist = %s\n", dirListing);
	if(dirListing != NULL)
		free(dirListing);
	dirListing = NULL;
	if(ent != NULL)
		free(ent);
	ent = NULL;
}

/*
void sort(struct Stats **files, int count, char *args)
{
	struct files **a, **b;
	if(args == NULL)
		qsort((void **)files, count, args, strcmp(a->d_name, b->d_name));
	else if(args == 2)
		qsort((void **)files, count, args, strcmp(a->d_name, b->d_name));
}*/

void processResponse(int fd, char* protocol, char* path, char * real_path, int code, char* type, int dir)
{
	printf("");
	//write(fd, "in processResponse", 18);
	write(fd, "HTTP/1.1 ", 9);
	if(code == 200)
		write(fd, "200 Document follows", 20);
	else if(code == 404)
		write(fd, "404 File Not Found", 18);
	write(fd, "\r\n", 2);
	write(fd, "Server: CS 252 lab5\r\n", 21);
	write(fd, "Content-type: ", 14);
	switch(type[0])
	{
		case 'g': write(fd, "image/gif", 9);
			break;
		case 'j': write(fd, "image/jpeg", 10);
			break;
		case 'h': write(fd, "text/html", 9);
			break;
		case 'i': write(fd, "image/x-icon", 12);
			break;
		default: if(!dir) write(fd, "text/plain", 10);
			break;
	}
	write(fd, "\r\n", 2);
	write(fd, "\r\n", 2);
	if(code == 200)
	{
		if(dir != 0)
			dirListing(path, real_path, fd);
		else
		{
			char * buf = (char *)malloc(sizeof(char)*128);
			FILE* infile = fopen(path, "rb");
			int i=0;
			while( i = fread(buf, sizeof(char), 128, infile))
			{
				if(i < 1)
					break;
				write(fd, buf, i);
				if(i < 128)
					break;
			}
			if(buf!=NULL)
				free(buf);
			else
				buf = NULL;
			fclose(infile);
		}
	}
	else if(code == 404)
		write(fd, "File Not Found", 14);
}

void processCGIBin(int socket, char * real_path, char * args)
{
	printf("in cgibin");
	const char *message = "HTTP/1.0 200 Document follows";
	write(socket, message, strlen(message));
	write(socket, "\r\n", 2);
	//write(socket, "Content-type: text/html\r\n", 23);
	printf("real path = %s\n", real_path);
	if(strncmp(&real_path[strlen(real_path)-3], ".so", 3)==0)
	{
		printf("Loadable modules");
		//Loadable modules
	}
	else
	{
		pid_t ret = fork();
		if(ret == 0)
		{
			printf("in child");
			printf("args = %s\n", args);
			if(args != NULL)
			{
				printf("args !=nUll\n");
				setenv("REQUEST_METHOD", "GET", 1);
				setenv("QUERY_STRING", args, 1);
			}
			//const char *message = "HTTP/1.0 200 Document follows\r\n";
			//write(socket, message, 31);
			//write(socket, "Content-type: text/html", 23);
			dup2(socket, 1);
			//close(socket);
		/*	write(fd, "HTTP/1.1 ", 9);
			
			if(code == 200)
				write(fd, "200 Document follows", 20);
			else if(code == 404)
				write(fd, "404 File Not Found", 18);
			write(fd, "\r\n", 2);
			write(fd, "Server: CS 252 lab5", 21);
			write(fd, "Content-type: ", 14);
		*/
			if(args != NULL)
				execl(real_path, args, 0, (char *)0);
			else
				execl(real_path, NULL, 0, (char *)0);
			close(socket);
			exit(0);
		}
		waitpid(ret, NULL, 0);
		shutdown(socket, SHUT_RDWR);
		close(socket);
	}
}

void
processTimeRequest( int fd )
{
  // Buffer used to store the name received from the client
  const int MaxName = 1024;
  char name[ MaxName + 1 ];
  int nameLength = 0;
	int num = 0;
	bool get = false;
	bool done = false;
	bool check = false;
	char * buf = (char *)malloc(MaxName * sizeof(char));
	//char full_name[1025];
	//vector<string> vec;

	char * response = NULL;
	char * path = (char *)malloc(MaxName + 1);
	char * requestType = (char *)malloc(MaxName + 1);
	char * type = (char *)malloc(MaxName + 1);
	char * protocol = (char *)malloc(9);
	char * root = NULL;
	memset(path, '\0', MaxName + 1);
	memset(requestType, '\0', MaxName + 1);
	memset(type, '\0', MaxName + 1);
	memset(protocol, '\0', 9);

	memcpy(protocol, (char *)"HTTP/1.0", 8);

  // Send prompt
  //const char * prompt = "\nType your name:";
  //write( fd, prompt, strlen( prompt ) );

  // Currently character read
  unsigned char newChar;
  unsigned char oldChar;

  // Last character read
  //unsigned char lastChar = 0;

  //
  // The client should send <name><cr><lf>
  // Read the name of the client character by character until a
  // <CR><LF> is found.
  //

	//strcpy(name,"http-root-dir");
	//nameLength = strlen(name);

	char newLine[MaxName];
	int n;
	int m = 0;

	printf("before parsing");

	printf("fd = %d\n", fd);

	while ( n = read( fd, &newChar, sizeof(newChar)) && newChar != ' ')
	{
		printf("m = %d\n", m);
		requestType[m] = newChar;
		printf("newChar = %c\n", requestType[m]);
		m++;
	}
	requestType[m] = '\0';
	printf("request type = %s\n", requestType);
	if(n < 0)
	{
		perror("Request parse");
		exit(0);
	}
	printf("after request type\n");
	if(strcmp(requestType, "GET") == 0)
	{
		printf("in get");
		int i = 0;
		int j = 0;
		int extension  = 0;
		while( n = read( fd, &newChar, sizeof(newChar)) > 0 && newChar != ' ')
		{
			if(extension)
			{
				type[j] = newChar;
				j++;
			}
			if(newChar == '.')
				extension = 1;
			path[i] = newChar;
			i++;
			printf("newChar = %c\n", newChar);
		}
		path[i] = '\0';
			
		if(strncmp(path, secretkey, strlen(secretkey))!=0)
		{
			//perror("Invalid secret key");
			//exit(0);
		}
		char * path1;
		path1 = path + strlen(secretkey);
		char a[200];
		//sprintf(a,"path = %s", path);
		//write(fd, a, strlen(a));
		while( read( fd, &newChar, sizeof(newChar)))
		{
			if(newChar == 10 && oldChar == 13)
			{
				read( fd, &newChar, sizeof(newChar));
				if(newChar == 13)
				{
					read(fd, &newChar, sizeof(newChar));
					if(newChar == 10)
						break;
				}
			}
			oldChar = newChar;
		}
		char dir[256] = {0};
		printf("before cwd");
		char * cwd = getcwd(dir, sizeof(dir));
		char * front = (char *)malloc(256);
		strcat(front, cwd);
		printf("after strcat");
		char * key = (char *)malloc(256);;
		bool isCGI = false;
		char * params = (char *)malloc(256);
		
		//key[0] = '/';
		//strcat(key, secretkey);
		//printf("key =  %s\n", key);
		//char * icons = strcat(key, "/icons");
		//char * htdocs = strcat(key, "/htdocs");
		//char * cgibin = strcat(key, "/cgi-bin");
		//char * slash = strcat(key, "/");
		//printf("icons = %s, htdocs = %s, cgibin = %s, slash = %s\n", icons, htdocs, cgibin, slash);
		printf("\nkey = %s, path = %s\n", key, path1);
		if(strncmp(path1, "/icons", 6)==0)
		{
			printf("in icons\n");
			printf("cwd = %s\n", cwd);
			root = (char *)malloc(256);
			strcpy(root, "/http-root-dir");
			strcat(root, path1);
			strcat(cwd, root);
			strcpy(root, cwd);
			//memset(root, '\0', strlen(cwd));
			printf("root = %s\n", root);
		}
		else if(strncmp(path1,"/htdocs", 7)==0)
		{
			printf("in htdocs\n");
			root = (char *)malloc(256);
			strcpy(root, "/http-root-dir");
			strcat(root, path1);
			strcat(cwd, root);
			strcpy(root, cwd);
			printf("root = %s\n", root);
		}
		else if(strncmp(path1, "/cgi-bin", 8)==0)
		{
			printf("in cgibin\n");
			root = (char *)malloc(256);
			//strcpy(root, "/cgi-bin");
			//strcat(root, path1);
			strcat(root, cwd);
			strcat(root, path1);
			isCGI = true;
			printf("root = %s\n", root);
		}
		else if(strcmp(path1, "/") == 0)
		{
			root = (char *)malloc(31);
			memset(root, '\0', 31);
			memcpy(root, "http-root-dir/htdocs/index.html", 31);
			strcpy(type, "html");
		}
		else
		{
			printf("in else\n");
			root = (char *)malloc(256);
			strcpy(root, "/http-root-dir/htdocs");
			strcat(root, path1);
			strcat(cwd, root);
			strcpy(root, cwd);
			printf("root = %s\n", root);
		}
		//char * real_path = (char *)malloc(256);
		char * ptr = realpath(root, real_path);
		if(!isCGI)
			strcat(front, "/http-root-dir");
		printf("root = %s\n", root);
		printf("real path = %s\n", real_path);
		printf("front = %s\n", front);
		if(strncmp(front, real_path, strlen(front))!=0)
		{
			perror("Invalid file path");
			exit(0);
		}
		printf("before cgi\n");
		if(isCGI)
		{
			bool hasArgs = false;
			char * args;
			int position = 0;
			for(int i=0;i<strlen(path1);i++)
			{
				if(path1[i] == '?')
				{
					hasArgs = true;
					args = (char *)malloc((strlen(path1)-i-1)*sizeof(char));
				}
				else if(hasArgs)
				{
					args[position] = dir[i];
					position++;
				}
			}
			if(hasArgs == false)
				args = NULL;
			processCGIBin(slaveSocket, root, args);
		}
		printf("after cgi\n");
	}
	else
		return;

	printf("b4 index\n");
	if(open(root, O_RDONLY) <= 0 && strstr(root, "index.html"))
	{
		int i = strlen(root) - 1;
		while(root[i] != '/')
		{
			root[i] = '\0';
			i--;
		}
	}
	printf("afer index\n");
	if(opendir(root))
		processResponse(fd, protocol, root, real_path, 200, type, 1);
	else
	{
		if(open(root, O_RDONLY) > 0)
			processResponse(fd, protocol, root, real_path, 200, type, 0);
		else
			processResponse(fd, protocol, root, real_path, 404, type, 0);
	}
	printf("after opendir root\n");
	close(fd);
	printf("b4 root\n");
	if(root != NULL)// && root != 0)
		free(root);
	else
		root = NULL;
	printf("after root\n");
	if(requestType != NULL)// && requestType != 0)
		free(requestType);
	else
		requestType = NULL;
	printf("after requestTye\n");
	if(path != NULL)// && path != 0)
		free(path);
	else
		path = NULL;
	printf("after path\n");
	if(type != NULL)// && type != 0)
		free(type);
	else
		type = NULL;
	printf("afer type\n");
	if(protocol != NULL)// && protocol != 0)
		free(protocol);
	else
		protocol = NULL;
	printf("afer protocol\n");
}

/*  while ( nameLength < MaxName && ( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
	while((n = read(fd, &newChar, sizeof(newChar))))
	{
		int m = 1024;
		buf[m++] = newChar;
		if(m >= num)
		{
			num = num * num;
			buf = (char *) realloc(buf, m);
		}
		if(oldChar == '\r' && newChar == '\n')
		{
			if(m > 3)
			{
				if(buf[num-3] == '\n' && buf[num -2] == '\r')
					break;
			}
		}
		oldChar = newChar;
	}
	free(buf);

	//buf[ nameLength ] = newChar;
	name[ nameLength ] = newChar;
    	nameLength++;
	printf("name = %s\n", name);

  	if(check)
	{
		char c;
		//strcat(full_name, newChar);
		//while((n = read(fd, &newChar, sizeof(newChar))))
		//{
			if(newChar == ' ')
			{
				done = true;
				//strcat(full_name, "\0");
				//break;
			}
			printf("name1 = %s\n", name);
			//buf[nameLength] = newChar;
			name[nameLength] = newChar;
			nameLength++;
			//strcat(full_name, (char *)newChar);
			printf("name2 = %s\n", name);
		//}
	}
	printf("name3 = %s\n", name);
	char real_name[1024];
	int space = 0;
	for(int i=0;i < strlen(name);i++)
	{
		if(name[i] == ' ')
			space++;
		else
		{
			if(space == 1)
				real_name[i] = name[i];
		}
	}
	if(done)
	{
		done = false;
		check = false;
	}
	if(newChar == ' ')
	{
		if(get)
		{
			get = false;
			check = true;
		}
	}
	else if(newChar == 'T')
	{
		if(name[nameLength-3] == 'G' && name[nameLength-2] == 'E')
			get = true;
	}
	//Break once you hit \r\n\r\n
	else if(newChar == '\n')
	{
		if(name[nameLength-4] == '\r' && name[nameLength-3] == '\n' && name[nameLength-2] == '\r')
			break;
	}
  
}
	printf("GET request: %s\n", name);

	//Map document path to real path

	//current working directory
	char dir[256];
	getcwd(dir, 256);
	strcat(dir, "/http-root-dir/htdocs/index.html");
	printf("dir = %s\n", dir);

	if(strncmp(name, "/icons", 6) && strncmp(name, "/htdocs", 7) && strncmp(name, "/cgi-bin", 8))
	{
		if(!strcmp(name, ".."))
		{
			strcat(dir, "/http-root-dir/htdocs/");
			strcat(dir, name);
		}
		else if(!strcmp(name, "/"))
		{
			strcpy(name, "/index.html");
			strcat(dir, "/http-root-dir/htdocs/");
			strcat(dir, name);
		}
		else
		{
			strcat(dir, "/http-root-dir/htdocs/");
			strcat(dir, name);
		}
	}
	else
	{
		//strcat(dir, "/http-root-dir/");
		strcat(dir, name);
	}

	if(strstr(name, "..") != 0)
	{
		char res[MaxName + 1];
		char * path = realpath(name, res);
		if(path == NULL)
		{
			perror("realpath");
			exit(0);
		}
	}

	char type[MaxName + 1];
	if(strstr(name, ".gif") != 0)
		strcpy(type, "image/gif");
	else if(strstr(name, ".html") != 0)
		strcpy(type, "text/html");
	else if(strstr(name, ".jpg") != 0)
		strcpy(type, "image/jpeg");
	else
		strcpy(type, "text/plain");

	FILE * infile = fopen(dir, "rb");

	fprintf(stderr, "dir = %s\n", dir);
	printf("dir = %s\n", dir);

	if(infile != NULL)
	{
		printf("file = %s", infile);
		write(fd, "HTTP/1.1 200 ", 29);
		write(fd, "\r\n", 2);
		write(fd, "Server: CS 252 lab5", 19);
		write(fd, "\r\n", 2);
		write(fd, "Content-type: ", 14);
		write(fd, type, strlen(type));
		write(fd, "\r\n", 2);
		write(fd, "\r\n", 2);

		int size = 0;
		char ch;
		while(size = read(fileno(infile), &ch, sizeof(ch)))
		{
			if(write(fd, &ch, sizeof(ch)) != size)
			{
				perror("write");
				return;
			}
		}
		fclose(infile);
	}
	else
	{
		printf("no file");
		write(fd, "HTTP/1.1 404 File Not Found", 27);
		write(fd, "\r\n", 2);
		write(fd, "Server: MyServer/1.0", 20);
		write(fd, "\r\n", 2);
		write(fd, "Content-Type: text/html", 23);
		write(fd, "\r\n", 2);
		write(fd, "\r\n", 2);
		write(fd, "File Not Found", 14);
	}
}*/
/*
  // Add null character at the end of the string
  name[ nameLength ] = 0;

  printf( "name=%s\n", name );

  // Get time of day
  time_t now;
  time(&now);
  char	*timeString = ctime(&now);

  // Send name and greetings
  const char * hi = "\nHi ";
  const char * timeIs = " the time is:\n";
  write( fd, hi, strlen( hi ) );
  write( fd, name, strlen( name ) );
  write( fd, timeIs, strlen( timeIs ) );
  
  // Send the time of day 
  write(fd, timeString, strlen(timeString));

  // Send last newline
  const char * newline="\n";
  write(fd, newline, strlen(newline));
*/ 

