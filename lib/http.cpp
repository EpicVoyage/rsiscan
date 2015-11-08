#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "http.h"

char *http::retrieve(char *server, char *path)
{
	FILE *wr, *re;
	char *ret;
	int sock;

	if (sock = this->connect(server)) {
		wr = fdopen(sock, "w");
		re = fdopen(sock, "r");

		this->send_request(wr, server, path);
		ret = this->retrieve_data(re);

		if ((ret != NULL) && (strcasestr(ret, "Sorry, the page you requested was not found.")))
		{
			// TODO
			//printf("Error: %s - No data found, delisting stock\n", ticker);
			//delist(ticker);
			free(ret);
			ret = NULL;
		}

		fclose(wr);
		fclose(re);
		close(sock);
	}

	return ret;
}

int http::connect(char *server, int port)
{
	struct hostent *record;
	struct sockaddr_in sin;
	int sock, conn;

	if ((record = gethostbyname(server)) == NULL)
	{
		fprintf(stderr, "DNS lookup for %s failed!\n", server);
		return 0;
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)record->h_addr));

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		fprintf(stderr, "Failed to open a socket!\n");
		return 0;
	}

	if ((conn = ::connect(sock, (struct sockaddr *)&sin, sizeof(sin))) < 0)
	{
		fprintf(stderr, "Error connecting to %s: %s\n", server, strerror(errno));
		return 0;
	}

	return sock;
}

void http::send_request(FILE *wr, char *server, char *path)
{
	fprintf(wr, "GET %s HTTP/1.1\r\n", path);
	fprintf(wr, "Host: %s\r\n", server);
	fprintf(wr, "Connection: close\r\n\r\n");
	fflush(wr);

	return;
}

char *http::retrieve_data(FILE *re)
{
	bool chunked = false, start = false;
	long x, len, size, content = 0;
	char *read, *tmp, *ret = NULL;

	size = 2048;
	read = (char *)malloc(size);
	while (fgets(read, size, re) != NULL)
	{
		if (!start)
			chunked = ((char *)strcasestr(read, "Transfer-Encoding: chunked") == NULL);

		if ((!start) && (strncasecmp(read, "Content-Length: ", 16) == 0) && (isdigit(*(read + 16))))
			content = strtol(read + 16, NULL, 10);

		if ((*read == '\r') || (*read == '\n'))
		{
			if ((content) && (ret == NULL))
			{
				ret = (char *)malloc(content + 1);
				if ((x = fread(ret, 1, content, re)) < content)
				{
					*(ret + x + 1) = 0;
					start = true;
				}
				else
				{
					*(ret + x + 1) = 0;
					break;
				}
			}
			else if (chunked)
			{
				if (fgets(read, size, re) != NULL)
				{
					len = strtol(read, NULL, 16);

					/* TODO: Why is Yahoo! off by 50 bytes? */
					if (this->yahoo_bug)
						len -= 50;

					if (len <= 0)
						continue;

					if (ret == NULL)
					{
						ret = (char *)malloc(len + 1);
						*ret = '\0';
					}
					else
						ret = (char *)realloc(ret, strlen(ret) + len + 1);

					if (len >= size)
					{
						size = len + 1;
						read = (char *)realloc(read, size);
					}

					fgets(read, size, re);
					if (x = fread(read, 1, len, re))
					{
						*(read + x) = 0;
						strcat(ret, read);
					}
				}
			}
			else
				start = true;
		}
		else if ((*read == '0') && (!strchr(read, ',')))
		{
			if (ret != NULL)
			{
				tmp = ret + strlen(ret) - 1;
				if (*tmp == '\n') *tmp = '\0';
				break;
			}
		}
		else if (start)
		{
			if (ret == NULL)
			{
				ret = (char *)malloc(strlen(read) + 1);
				*ret = '\0';
			}
			else
				ret = (char *)realloc(ret, strlen(ret) + strlen(read) + 1);

			strcat(ret, read);
		}
	}

	free(read);
	return ret;
}
