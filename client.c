#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CNTPORT "3548"
#define MAX_BUF 2000
#define MAX_AW 10
#define PROT_CNT "CNT\n"
#define PROT_OK "OK\n"
#define PROT_WON "WON\n"

/* prototypes */
int is_in_answers(char *ts);

static char *possible_answers[] = { "A\n", "B\n", "C\n", "D\n" };
static char *possible_answers2 = "A, B, C, D";

int main(int argc, char **argv){
	int sockfd, numbytes;
	int question = 1;
	int rc, nlcnt, cnt = 0;
	struct addrinfo *server, *p, hints;
	char ipstr[INET6_ADDRSTRLEN];
	char qst_buf[MAX_BUF];
	char answer_buf[MAX_AW];
	char reply_buf[MAX_AW];

	if(argc != 2){
		fprintf(stderr, "usage: client <hostname/ipv4_6 address>\n");
		return EXIT_FAILURE;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	/* use ipv4 or v6 and use stream sockets */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rc = getaddrinfo(argv[1], CNTPORT, &hints, &server)) != 0){
		fprintf(stderr, "[client] Lookup of hostname %s wasn't successful\n",
				argv[1]);
		return EXIT_FAILURE;
	}

	/* try to bind to the first usable ip-port combination
	 	we can find */
	for(p = server; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype,
					p->ai_protocol)) == -1){
			fprintf(stderr, "[client] Socket creation failed with message%s\n", 
					strerror(errno));
			continue;
		}	

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			fprintf(stderr, "[client] Could not connect to host %s\n", argv[1]);
			fprintf(stderr, "[client] error message: %s\n", strerror(errno));
			continue;
		} else {
			printf("[client] Connection success!\n");
		}

		break;
	}

	if(p == NULL){
		fprintf(stderr, "[client] Could not connect to any host\n");
		return EXIT_FAILURE;
	}

	inet_ntop(p->ai_family, 
		p->ai_family == AF_INET ?
			(void *) &(((struct sockaddr_in *)p->ai_addr)->sin_addr) :
			(void *) &(((struct sockaddr_in6*)p->ai_addr)->sin6_addr),
		ipstr,
		sizeof(ipstr));

	printf("[client] Connected to host %s on port %d\n", ipstr, 
			ntohs(p->ai_family == AF_INET ?
				((struct sockaddr_in *)p->ai_addr)->sin_port :
				((struct sockaddr_in6*)p->ai_addr)->sin6_port));

	freeaddrinfo(server);

	/* send CNT*/

	if ((rc = send(sockfd, PROT_CNT, strlen(PROT_CNT), 0)) == -1) {
		fprintf(stderr, "[client] Error while sending message %s to server", 
				PROT_CNT);
		close(sockfd);
		return EXIT_FAILURE;
	} else if (rc != strlen(PROT_CNT)) {
		fprintf(stderr, "[client] Not all butes could be transmitted\n");	
		close(sockfd);
		return EXIT_FAILURE;
	}

	if ((rc = recv(sockfd, qst_buf, strlen(PROT_OK), 0)) == -1){
		fprintf(stderr, "[client] Error while receivning\n");
		close(sockfd);
		return EXIT_FAILURE;
	} else if (rc != strlen(PROT_OK)){
		fprintf(stderr, "[client] could not receive enough bytes\n");
		close(sockfd);
		return EXIT_FAILURE;
	}

	if(strcmp(qst_buf, PROT_OK)){
		fprintf(stderr, "[client] protocol error!\n");	
		close(sockfd);
		return EXIT_FAILURE;
	}

	while (true){
		char tempbuf[10];

		sprintf(tempbuf, "%d\n", question);
		if((rc = send(sockfd, tempbuf, strlen(tempbuf), 0)) == -1){
			fprintf(stderr, "[client] Error while sending request \"%s\"\n", tempbuf);	
			close(sockfd);
			return EXIT_FAILURE;
		} else if (rc != strlen(tempbuf)){
			fprintf(stderr, "[client] Could not send all characters of \"%s\"\n", tempbuf);
			close(sockfd);
			return EXIT_FAILURE;
		}

		cnt = 0;
		/* Get the next question*/
		while(true){
			rc = recv(sockfd, qst_buf + cnt, 1, 0);
			if(rc == -1){
				fprintf(stderr, "Error while receiving: %s\n", strerror(errno));
				close(sockfd);
				return EXIT_FAILURE;
			} else if (rc == 0){
				fprintf(stderr, "[client] Connection closed by server\n");
				close(sockfd);
				exit(EXIT_FAILURE);
			}

			if(*(qst_buf + cnt) == '\n'){
				nlcnt++;
				if(nlcnt == 2){
					nlcnt = 0;
					break;
				}
			} else {
				if(nlcnt != 0){
					nlcnt = 0;
				}
			}

			cnt++;
		}
		qst_buf[cnt] = '\0';

		printf("[client] Received question:\n%s", qst_buf);
		do {
			printf("Your answer ");
			printf("(%s): ", possible_answers2);
			fgets(answer_buf, MAX_AW -1, stdin);
		} while(!is_in_answers(answer_buf));

		if((rc = send(sockfd, answer_buf, strlen(answer_buf), 0)) == -1){
			fprintf(stderr, "[client] The answer could not be sended\n");
			fprintf(stderr, "[client] message: %s\n", strerror(errno));
			close(sockfd);
			return EXIT_FAILURE;
		} else if (rc != strlen(answer_buf)){
			fprintf(stderr, "[client] The answer could not be fully transmitted\n");
			close(sockfd);
			return EXIT_FAILURE;
		}

		cnt = 0;
		/* Get reply from Server. This reply is always terminated
			by one single '\n' */
		do {
			rc = recv(sockfd, reply_buf + cnt, 1, 0);
			if (rc == -1){
				fprintf(stderr, "[client] Error while receiving\n");
				fprintf(stderr, "[client] message: %s\n", strerror(errno));
				close(sockfd);
				return EXIT_FAILURE;
			} else if (rc == 0){
				fprintf(stderr, 
						"[server:%i] Connection closed by client!\n", 
						getpid());
				close(sockfd);
				return EXIT_FAILURE;
			}
		} while(cnt < sizeof(reply_buf) -1 && reply_buf[cnt++] != '\n');
		
		if(!strcmp(reply_buf, PROT_OK)){
			printf("[client] Okay, that was the right answer. Go on!\n");
		} else if (!strcmp(reply_buf, PROT_WON)){
			printf("[client] You won. Congratulations!!!\n");
			printf("[client] You're one great operating system specialist!\n");	
			close(sockfd);
			break;
		} else {
			printf("[clinet] You failed miserably. Try again\n");
			close(sockfd);
			break;
		}
		question++;
	}
}

int is_in_answers(char *ts){
	int result = 0;
	int iter;
	for(iter = 0; iter < sizeof(possible_answers)/sizeof(char*); iter++){
		if(!strcmp(possible_answers[iter], ts)){
			result = 1;
			break;
		}
	}

	return result;
}
