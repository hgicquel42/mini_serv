#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <netinet/in.h>

int maxid = 0;
int maxfd = 0;

int ids[65536];
char *msgs[65536];

char rbuf[1025];
char wbuf[42];

fd_set fds, rfds, wfds;

// From subject main.c
int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

// From subject main.c
char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int puterr(char *msg) {
    write(2, msg, strlen(msg));
    return 1;
}

int fatal() {
    return puterr("Fatal error\n");
}

void notify(int client, char *msg) {
    for (int fd = 0; fd <= maxfd; fd++)
        if (FD_ISSET(fd, &wfds) && fd != client)
            send(fd, msg, strlen(msg), 0);
}

int main(int argc, char **argv) {
    if (argc != 2)
        return puterr("Wrong number of arguments\n");

    FD_ZERO(&fds);
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1)
        return fatal();
    FD_SET(server, &fds);
    maxfd = server;

    struct sockaddr_in addr;
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(2130706433);
	addr.sin_port = htons(atoi(argv[1]));

    if (bind(server, (struct sockaddr*) &addr, sizeof(addr)) == -1)
        return fatal();
    if (listen(server, SOMAXCONN) == -1)
        return fatal();
    while (42) {
        rfds = wfds = fds;
        if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) == -1)
            return fatal();
        for (int fd = 0; fd <= maxfd; fd++) {
            if (!FD_ISSET(fd, &rfds))
                continue;
            if (fd == server) {
                int client = accept(server, NULL, NULL);
                if (client == -1)
                    continue;
                if (client > maxfd)
                    maxfd = client;
                ids[client] = maxid++;
                msgs[client] = NULL;
                FD_SET(client, &fds);
                sprintf(wbuf, "server: client %d just arrived\n", ids[client]);
                notify(client, wbuf);
            } else {
                int client = fd;
                int n = recv(client, rbuf, 1024, 0);
                if (n <= 0) {
                    sprintf(wbuf, "server: client %d just left\n", ids[client]);
                    notify(client, wbuf);
                    FD_CLR(client, &fds);
                    free(msgs[client]);
                    msgs[client] = NULL;
                    close(client);
                } else {
                    rbuf[n] = '\0';
                    msgs[client] = str_join(msgs[client], rbuf);
                    char *msg;
                    while (extract_message(msgs + client, &msg)) {
                        sprintf(wbuf, "client %d: ", ids[client]);
                        notify(client, wbuf);
                        notify(client, msg);
                        free(msg);
                        msg = NULL;
                    }
                }
            }
            break;
        }
    }
    return (0);
}