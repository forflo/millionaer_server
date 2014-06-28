#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "server.h"

static struct settings *s_settings;


static int init_settings(){
	struct settings *s = malloc(sizeof(struct settings));
	if(s == NULL){
		fprintf(stderr, "[server] Speicherfehler\n");
		return -1;
	}	
	s->version = false;
	s->help = false;
	s->port = atoi(BINDPORT);
	s->portstr = BINDPORT;

	s_settings = s;
	return 0;
}

int parse_args(int argc, char **argv){
	int temp;
	int c, opt_i;
	if(init_settings()){
		fprintf(stderr, "[server] Could not initialize commandline parser\n");
		return -1;
	}

	static struct option opts[] = {
		{"version", no_argument, 0, 'v'},	
		{"help", no_argument, 0, 'h'},	
		{"port", required_argument, 0, 'p'}
	};

	while((c = getopt_long(argc, argv, OPTS, opts, &opt_i)) != -1){
		switch(c){
			case 'v':
				s_settings->version = true;
				break;
			case 'h':
				s_settings->help = true;
				break;
			case 'p':
				for(temp = 0; temp < strlen(optarg); temp++){
					if(optarg[temp] < 48 || optarg[temp] > 57){
						fprintf(stderr, "[server] invalid portnumber\n");
						return -1;
					}
				}
				s_settings->portstr = malloc(sizeof(char) * (strlen(optarg) + 1));
				if(s_settings->portstr == NULL){
					fprintf(stderr, "[server] Failure during memory allocation");
					return -1;
				}

				strcpy(s_settings->portstr, optarg);
				s_settings->port = atoi(optarg);
				break;
		}
	}
	
	return 0;
}

bool is_version(){ return s_settings->version; }
bool is_help(){ return s_settings->help; }
int	get_port(){ return s_settings->port; }
char *get_portstr() {return s_settings->portstr; }
