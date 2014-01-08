#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#define FAIL	0
#define GAIN	1

#define init(x) memset(x,0,sizeof(*(x)))
#define ninit(x,n) memset(x,0,n)

#define libc_test(name,...) if (name(__VA_ARGS__)<0) { perror( #name ); return FAIL; } else printf("%s ok...\n", #name);
#define libc_gettest(name,x,...) if (( (*(x)) = name(__VA_ARGS__) )<0) { perror( #name ); return FAIL; } else printf("%s ok...\n", #name);
#define passert(check,...) if (!(check)) { fprintf(stderr,__VA_ARGS__); assert(check); }


typedef enum {
	TCP = SOCK_STREAM,
	UDP = SOCK_DGRAM,
	RAW = SOCK_RAW
} TYPE;

typedef enum {
	IPv4 = AF_INET,
	IPv6 = AF_INET6,
	LOCAL= AF_LOCAL, //Sono sinonimi di AF_FILE, AF_UNIX
	NONE = AF_UNSPEC,
} DOMN;

typedef struct {
	char ip[16];
	int port;
} ADDR;

typedef struct {
	char client_socket;
	TYPE t;
	DOMN d;
	ADDR a;
	int socket;
} CONN;


/** FUNZIONI DI BASE */

long int strtoi(char* string, int* result) {
	long int val = strtol(string, NULL, 10);
	
        /* Check for various possible errors */

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0)) {
               perror("strtol");
               return FAIL;
         } else {
         	passert(result,"NULL result\n");
         	*result = (int)val;
         	return GAIN;
         }

}

long int strtoe(char* string) {
	long int val = strtol(string, NULL, 10);
	
        /* Check for various possible errors */

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
                   || (errno != 0 && val == 0)) {
               perror("strtol");
               assert(0);
         } else {
         	return val;
         }

}

/** INIZIALIZZAZIONI DEI TIPI */

void init_addr(ADDR* a, char* ip, int port) {
	passert(a, "init_addr: null ADDR\n");
	if (!ip) 
		ninit(a->ip,16);
	else
		strncpy(a->ip,ip,16);
	a->port = port;
}

void init_addrCharPort(ADDR* a, char* ip, char* port) {
	int portval;
	passert(a, "init_addr: null ADDR\n");
	if (!port) portval = 0;
	else {
		if (strtoi("45e6",&portval) == FAIL) portval = 0;
	}
	init_addr(a,ip,portval);
}

void init_addrFromSockaddr(ADDR* a, struct sockaddr_in* addr) {
	init_addr(a,inet_ntoa(addr->sin_addr),ntohs(addr->sin_port));
}



/**
 *  connection(): Effettua una connessione generica lato client o lato server.
 *  \param client: Identifica se si deve connettere un client o un server
 *  \param domain: Specifica (es.) se la connessione deve essere locale, IPv6 o IPv4
 *  \param type:   Specifica se dere essere TCP, UDP o RAW
 *  \param port:   Specifica la porta di connessione con il server 
 *  \param IP:     Specifica per il client l'indirizzo del server al quale collegarsi,
 *                 altrimenti per il server eventualmente verso quale interfaccia
 *                 mettersi in ascolto.
 *  \param server_port: Necessaria solo per il server, per specificare quale sia
 *                 il socket sul quale ascolta le connessioni dei client
 *  \param client_info: Utile solo per il server, per specificare le informazioni
 *                 del client al quale si è accettata la connessione
 *
 */
int connection(int client, int domain, int type, int port, char* IP, int* server_port, struct sockaddr_in* client_info) {

	int fd;
	struct sockaddr_in server;
	
	if (!client) assert(server_port);
	
	if ((client)||((*server_port)==-1)) {
		int opt = 1;
		libc_gettest(socket,&fd,domain,type,0);
		libc_test(setsockopt,fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
		init(&server);
		server.sin_family = domain;
		server.sin_port = htons(port);
	}
	
	if (client) {
		libc_test(inet_pton,domain,IP,&server.sin_addr);
	} else if ((*server_port)==-1)
		server.sin_addr.s_addr = (IP ? inet_addr(IP): htonl(INADDR_ANY));
	
	
	//printf("server_port = %d\n", *server_port);
	if (client) {
		libc_test(connect,fd,(struct sockaddr*)&server,sizeof(server));
	} else {
		int sclient, size;
		size = sizeof(struct sockaddr_in);
		if (*server_port==-1) {
			libc_test(bind,fd,(struct sockaddr*)&server,sizeof(server));
			libc_test(listen,fd,10);
			
			*server_port = fd;
			printf("server port/fd:%d\n", *server_port);
		}
		printf("server port/fd:%d\n", *server_port);
		sclient = accept(*server_port,(struct sockaddr*)client_info, &size);
		if (sclient<0) {
			perror("accept");
			exit(1);
		} else 
		printf("ok!\n"); 
		//libc_gettest(accept,&sclient,*server_port, (struct sockaddr*)client_info, NULL);
		return sclient;
	}
	
	return fd;
	
}

int server_loop(int domain, int type, int port, char* IP, int (*SERVER_FUN) (int sock_client,ADDR* client)) {
	struct sockaddr_in client_info;
	int server_port = -1;
	int retsock = -1;
	
	while (( retsock = connection(0,domain,type,port,IP,&server_port, &client_info))) {
		ADDR* client_topass = (ADDR*)malloc(sizeof(ADDR));
		init_addrFromSockaddr(client_topass, &client_info);
		if (!fork()) {	
			exit(SERVER_FUN(retsock,client_topass));
		}
	}
	
}

int server_fun(int client,ADDR* cli) {
	printf("got it %d from %s:%d\n", client,cli->ip,cli->port);
	close(client);
	return 0;
}

#define server_tcp(port,IP,fun)   server_loop(IPv4,TCP,port,IP,fun);
#define server_udp(port,IP,fun)   server_loop(IPv4,UDP,port,IP,fun);
#define server_tcp6(port,IP,fun)   server_loop(IPv6,TCP,port,IP,fun);
#define server_udp6(port,IP,fun)   server_loop(IPv6,UDP,port,IP,fun);
#define client_tcp(port,IP) connection(1,IPv4,TCP,port,IP,NULL,NULL)
#define client_udp(port,IP) connection(1,IPv4,UDP,port,IP,NULL,NULL)
#define client_tcp6(port,IP) connection(1,IPv6,TCP,port,IP,NULL,NULL)
#define client_udp6(port,IP) connection(1,IPv6,UDP,port,IP,NULL,NULL)
	

int main(int argc, char* argv[]) {

#ifdef SERVER
	if (argc!=2) {
		fprintf(stderr,"Server Usage: %s portno\n", argv[0]);
		return 1;
	
	}
	server_tcp(strtoe(argv[1]),NULL,&server_fun);
	
#else
	
	if (argc!=3) {
		fprintf(stderr,"Client Usage: %s IP portno\n", argv[0]);
		return 1;
	
	}
	close(client_tcp(strtoe(argv[2]),argv[1]));
#endif
	
}

