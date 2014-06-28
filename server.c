/* 
 * Author: Florian Mayer
 * Date: 23-24.6.2014
 * Summary: This is the reimplementation of the well
 *   known "millionaers server" originally written by
 *   Ludwig Frank and continuously improved by Klaus
 *   Voggenauer. It is a complete rewrite based on a mealy 
 *   state automaton which i used for implementing the 
 *   protocol logic. 
 *	Notes: This version tries to catch up all potential
 *    errors produced by the common socket systemcalls,
 *    although this program assumes that a send call 
 *    always delivers every byte it has been given.
 */

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#define QUEUE 20
#define BINDPORT "3548"

#define WON "WON\n"

/* Some specifications for the used protocol 
	Note: Two newline characters terminate a question. 

	Protocol Commands:
	command			|
	----------------+------------------------
	"CNT\n"			| used do initiate a session 
	"OK\n"			| acknoledgement message from server
	"NOK\n"			| client lost
	"1\n".."15\n"	| client request for question n
	"WON\n"			| client won. session over
	"A\n".."B\n"	| client chooses answer a, b, c, or d */

#define QST1  "Die 50-Euro-Frage:\nWelcher Begriff beschreibt eine Betriebssystemschnittstelle?\n"\
				"A: Posix32		B: Win32\nC: Winner_16	D: Looser_8\n\n"
#define QST2  "Die 100-Euro-Frage:\nWelchen Prozesszustand gibt es in Betriebssystemen?\n"\
				"A: verhindert	B: agil\nC: bereit		D: verschlafen\n\n"
#define QST3  "Die 200-Euro-Frage:\nWas ist richtig? Die Hardware-Schnittstelle wird beschrieben im\n"\
				"A: IHS			B: HSI\nC: SJF			D: i386\n\n"
#define QST4  "Die 300-Euro-Frage:\nWas ist richtig? Die Semaphoroperation P wird in Linux realisiert im Systemaufruf\n"\
				"A: semctrl		B: semop\nC: semget		D: semdepp\n\n"
#define QST5  "Die 500-Euro-Frage:\nWelches Standardsynchronisationsproblem gibt es nicht?\n"\
				"A: Schlafender Barbier		B: Speisende Philosophen\nC: Schimpfender Professor	D: Leser-Schreiber\n\n"
#define QST6  "Die 1000-Euro-Frage:\nWelche Firma entwickelte das erste Betriebssystem?\n"\
				"A: Motorola	B: IBM\nC: Microsoft	D: Maxihard\n\n"
#define QST7  "Die 2000-Euro-Frage:\nWas ist richtig? Viele Rechner bilden zusammen eine(n)\n"\
				"A: Rechnerfarm	B: Rechnerpflanzung\nC: Rechnerfriedhof	D: Computerranch\n\n"
#define QST8  "Die 4000-Euro-Frage:\nWer ist der Erfinder der Semaphore?\n"\
				"A: Albert Einstein			B: Andrew Tanenbaum\nC: Edsger Dijkstra		D: Uwe Seeler\n\n"
#define QST9  "Die 8000-Euro-Frage:\nWelches Betriebssystem gab/gibt es nicht?\n"\
				"A: Rosix		B: Munix\nC: Xenix		D: ULTRIX\n\n"
#define QST10  "Die 16000-Euro-Frage:\nNach welchem Modell arbeiten mehrere Prozesse nicht zusammen?\n"\
				"A: Boss/Worker-Modell		B: Agent/Manager-Modell\nC: Pipeline-Modell	D: Girl-and-Boy-Modell\n\n"
#define QST11  "Die 32000-Euro-Frage:\nWas ist richtig? Ein Thread ist\n"\
				"A: ein Schwergewichtsprozess	B: ein Leichtgewichtsprozess\nC: ein blockierter Prozess"\
				"	D: ein Name fuer die Steuerregister eines Prozesses\n\n"
#define QST12  "Die 64000-Euro-Frage:\nWas ist richtig? Das Betriebssystem\n"\
				"A: schaltet die HW ein		B: virtualisiert die HW\nC: macht die HW ueberfluessig		D: verhunzt die HW\n\n"
#define QST13  "Die 125000-Euro-Frage:\nWie kann man in Betriebssystemen keinen Prozess im weitesten Sinne erzeugen?\n"\
				"A: clone					B: exec\nC: fork						D: pthread_create\n\n"
#define QST14  "Die 500000-Euro-Frage:\nWelcher der folgenden Begriffe bezeichnet ein Synchronisationsverfahren?\n"\
				"A: Stelldichein				B: Verabredung\nC: Rendezvous				D: Zweierkiste\n\n"
#define QST15  "Die 1000000-Euro-Frage:\nWas ist meine liebste Vorlesung?\n"\
				"A: Betriebssysteme			B: Betriebssysteme\nC: Betriebssysteme			D: Betriebssysteme\n\n"

/* prototypes */
void sigchld_handler(int s);
void client_worker(int newsock);

/* initial state */
int pstate = 0;
int state = 0;
int fz = 32;
int ez = 31;

struct mtok {
	char *token;
	char *output;	
};

/* Since the state machine below is a mealy-automaton, 
   	the input tokens have to be mapped to output tokens.
	mtok corresponds to m(apped)tok(ens) */
struct mtok tr[24] = {
	{"CNT", "OK\n"},
	{"1", QST1},
	{"2", QST2},
	{"3", QST3},
	{"4", QST4},
	{"5", QST5},
	{"6", QST6},
	{"7", QST7},
	{"8", QST8},
	{"9", QST9},
	{"10", QST10},
	{"11", QST11},
	{"12", QST12},
	{"13", QST13},
	{"14", QST14},
	{"15", QST15},
	{"A","OK\n"},
	{"B","OK\n"},
	{"C","OK\n"},
	{"D","OK\n"},
	{"A","NOK\n"},
	{"B","NOK\n"},
	{"C","NOK\n"},
	{"D","NOK\n"},
};

/* contains the complete protocol logic */
int state_machine[][33] = {
/*   1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 */
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //ok
	{0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //ok
	{0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, //ok	
	{0,0,0,0,0,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,10,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,12,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,20,0,0,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,22,0,0,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,24,0,0,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,26,0,0,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,28,0,0,0,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,0,0,0},	

	{0,0,0,0,0,0,0,0,0,0,0,0,13,0,15,0,0,0,19,0,0,0,0,0,0,0,0,0,0,0,31,0,0},	
	{0,0,3,0,0,0,7,0,9,0,0,0,0,0,0,0,0,0,0,0,0,0,23,0,25,0,27,0,0,0,31,0,0},	
	{0,0,0,0,5,0,0,0,0,0,11,0,0,0,0,0,17,0,0,0,0,0,0,0,0,0,0,0,29,0,31,0,0},	
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,21,0,0,0,0,0,0,0,0,0,31,0,0},	

	{0,0,32,0,32,0,32,0,32,0,32,0, 0,0, 0,0,32,0, 0,0,32,0,32,0,32,0,32,0,32,0,0,0,0},	
	{0,0, 0,0,32,0, 0,0, 0,0,32,0,32,0,32,0,32,0,32,0,32,0, 0,0, 0,0, 0,0,32,0,0,0,0},	
	{0,0,32,0, 0,0,32,0,32,0, 0,0,32,0,32,0, 0,0,32,0,32,0,32,0,32,0,32,0, 0,0,0,0,0},	
	{0,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0,32,0, 0,0,32,0,32,0,32,0,32,0,0,0,0},	
};

/* For better clarity the whole server logic is
	implemented in one main function. If you
 	do not like that, please consider unwrapping the
 	function */
int main(int argc, char **argv){
	int mysock, sockfd, newsock, rc;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;

	socklen_t sin_size;
	struct sigaction sa;

	/* used by inet_ntop. Will contain the 
	 	IPv4 or IPv6 address as string representation */	
	char ipstr[INET6_ADDRSTRLEN];
	
	/* configure default values for getaddrinfo */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC; //IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //default: TCP
	hints.ai_flags = AI_PASSIVE; //use localhost

	if((rc = getaddrinfo(NULL, BINDPORT, &hints, &servinfo)) != 0){
		fprintf(stderr, 
				"Could not get local address information: %s\n", 
				gai_strerror(rc));			
		return EXIT_FAILURE;
	}

	/* connect to the first ip-port combination we can find */
	for(p = servinfo; p != NULL; p = p->ai_next){
		int yes=1;
		/* create new socket */
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("Error while getting Socket:\n");
			continue;
		}

		/* No more "socket already in use" error messages while restarting
		 	the server */
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
			perror("Error while setting a socket option:\n");
			return EXIT_FAILURE;
		}

		/* Bind socket sockfd to Port 7778 on localhost */
		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("Error while binding to local address:\n");
			return EXIT_FAILURE;
		}

		break;
	}

	if(p == NULL){
		fprintf(stderr, "[server]: Failed to bind to any port\n");
		return EXIT_FAILURE;
	}	

	/* servinfo is no longer needed */
	freeaddrinfo(servinfo);

	/* Listen on Port 7778 on localhost */
	if(listen(sockfd, QUEUE) == -1){
		perror("Failed to establish listening on socket\n");
		return EXIT_FAILURE;
	}

	/* configure signal handler*/
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = sigchld_handler;
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1){
		perror("sigaction");
		return EXIT_FAILURE;
	}

	printf("[server]: started and listening for connections on %s...\n", BINDPORT);	

	/* accept loop */
	while(true){
		sin_size = sizeof(struct sockaddr_storage);
		if((newsock = accept(sockfd, (struct sockaddr *) 
										&their_addr, &sin_size)) == -1){
			perror("Error while accepting a client\n");
			break;
		}

		/* Transforms the ip address inside of their_addr into
			a string */
		inet_ntop(their_addr.ss_family,
			their_addr.ss_family == AF_INET ?
				(void *) &(((struct sockaddr_in *)&their_addr)->sin_addr) :
				(void *) &(((struct sockaddr_in6*)&their_addr)->sin6_addr),
			ipstr,
			sizeof(ipstr));	

		printf("[server]: accepted a connection from %s\n", ipstr);
	
		/* Fork and work the client */
		if(!fork()){
			/* child doesn't need sockfd */
			close(sockfd);
			client_worker(newsock);
		} else {
			/* in parent*/
			printf("[server:parent] Closing newsock of server\n");
			close(newsock);
		}
	}

	printf("[server:parent] Shutting down!\n");
	return EXIT_SUCCESS;
}

void client_worker(int newsock){
	int len;
	int sendc, retc, cnt, i;
	char rbuf[5] = { 0 }; //initializing to zero

	/* main protocol implementation */
	while(true){
		cnt = 0;
		/* Get request from Client */
		do {
			retc = recv(newsock, rbuf + cnt, 1, 0);
			if (retc == -1){
				perror("Error while receiving:\n");
				close(newsock);
				exit(EXIT_FAILURE);
			} else if (retc == 0){
				fprintf(stderr, 
						"[server:%i] Connection closed by client!\n", 
						getpid());
				close(newsock);
				exit(EXIT_FAILURE);
			}
		} while(cnt < sizeof(rbuf) -1 && rbuf[cnt++] != '\n');

		/* replace the \n with \0 because we don't need it*/
		rbuf[cnt-1] = '\0';

		printf("[server:%i] read following string: \"%s\"\n", 
				getpid(), rbuf);

		for(i=0; i<sizeof(tr)/sizeof(struct mtok); i++){
			/* Search for the token that transfers the state 
			   automaton to the next state */
			if(!strcmp(tr[i].token, rbuf)){
				/* Set the new state of the state automaton */
				pstate = state;
				state = state_machine[i][state];
				printf("[server(state machine):%i] Changing to state %i using i: %i\n", 
						getpid(), state, i);

				/* check if we have a generally false input from the 
				   client*/
				if(state == 0){
					/* since there are two tokens A and A but with 
					   different output, we have to traverse the 
					   state_machine table further until we have 
					   reached the end */
					int j;
					for (j=i; j<sizeof(tr)/sizeof(struct mtok); j++){
						if(!strcmp(tr[j].token, rbuf)){
							state = state_machine[j][pstate];

							if(state == 0){
								fprintf(stderr, 
										"[server:%i] Protocol error!\n", 
										getpid());
							} else if (state == fz) {
								if(send(newsock, tr[j].output, strlen(tr[j].output), 0 ) == -1){
									fprintf(stderr, "[server:%i] Error while sending NOK!\n", getpid());
									exit(EXIT_FAILURE);
								}
							}
						}
					}
					break;

					/* check if the internal state is fz (failure state) or 
					   ez (end state) */
				} else if (state == ez){
					if(send(newsock, WON, strlen(WON), 0) == -1){
						fprintf(stderr, 
								"[server:%i] Error while sending WON!\n", 
								getpid());
									exit(EXIT_FAILURE);
					}

					break;
				} else if (state == fz){
					if(send(newsock, tr[i].output, strlen(tr[i].output), 0 ) == -1){
						fprintf(stderr, 
								"[server:%i] Error while sending NOK!\n", 
								getpid());
									exit(EXIT_FAILURE);
					}

					break;

					/* New state is valid and not fz or ez. 
					   Send corresponding output to client */
				} else {
					printf("[server:%i] Accepting token! Output is: %s\n",
							getpid(), tr[i].output);
					len = strlen(tr[i].output);
					/* Send the question or the error */
					sendc = send(newsock, tr[i].output, len, 0);

					if(sendc == -1){
						fprintf(stderr, 
								"[server:%i] Error while sending the requested question #%i!\n", 
								getpid(), i);
									exit(EXIT_FAILURE);
					} else if(sendc != len) {
						fprintf(stderr, 
								"[server:%i] Not all bytes could be \
								sent!\n", getpid());
									exit(EXIT_FAILURE);
					}

					break;
				}
			}
		}

		/* check whether we have reached a final or error state yet */
		if(state == 0){
			printf("[server:%i] Closing because of protocol error\n!", 
					getpid());
			close(newsock);
			break;
		} else if (state == ez){
			printf("[server:%i] Game successfully finished\n!", getpid());
			break;
		} else if (state == fz){
			printf("[server:%i] Game ended tragically!\n",getpid());
			break;
		} 
	}

	printf("[server:%i] Closing socket!\n", getpid());
	if(close(newsock) == -1){
		fprintf(stderr, "[server:%i] Error closing socket!\n", getpid());
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
} 

/* prevent zombie processes */
void sigchld_handler(int s){
	while(waitpid(-1, NULL, WNOHANG) > 0);
}
