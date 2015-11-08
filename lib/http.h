#include <stdio.h>

/**
 * Retrieve data from a remote server.
 */
class http
{
public:
	bool yahoo_bug;
	http() { yahoo_bug = false; };
	char *retrieve(char *server, char *path);

private:
	int connect(char *server, int port = 80);
	void send_request(FILE *wr, char *server, char *path);
	char *retrieve_data(FILE *re);
};
