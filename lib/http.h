#include <stdio.h>

/**
 * Retrieve data from a remote server.
 */
class http
{
public:
	char *retrieve(const char *server, const char *path);

private:
	int connect(const char *server, int port = 80);
	void send_request(FILE *wr, const char *server, const char *path);
	char *retrieve_data(FILE *re);
};
