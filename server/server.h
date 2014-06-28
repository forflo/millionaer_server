#define QUEUE 20
#define BINDPORT "3548"

#define WON "WON\n"
#define OPTS "vhp:"

struct settings {
	bool version;
	bool help;
	int port;
	char *portstr;
};

int parse_args(int argc, char **argv);

bool is_version();
bool is_help();
int	get_port();
char *get_portstr();

/* server.c prototypes */
void sigchld_handler(int s);
void client_worker(int newsock);

int version_info();
int help_info();

