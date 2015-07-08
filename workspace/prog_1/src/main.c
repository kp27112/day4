#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

#define BUFF_SIZE 1024
int CLIENTS = 256;
int srv_fd = -1;
int epoll_fd = -1;
struct epoll_event e;

typedef struct user {
	int fd;
	char* name;
}user;

user *users[256];
int add_user(int fd, char* name);
user* find_user_by_fd(int fd);
user* find_user_by_name(char* name);

void sig_handler(int signo)
{
	if(epoll_fd)
		close(epoll_fd);
	if(srv_fd)
		close(srv_fd);
}

int handle_server()
{
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len;
	int cli_fd = -1;

	memset(&cli_addr_len, 1, sizeof(cli_addr_len));
	memset(&cli_addr, 0, sizeof(cli_addr));

	cli_fd = accept(srv_fd, (struct sockaddr*) &cli_addr, &cli_addr_len);

	if (cli_fd < 0)
	{
		printf("Cannot accept client\n");
		perror("Epoll error : ");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	e.data.fd = cli_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_fd, &e) < 0) {
		printf("Cannot accept client\n");
		perror("Epoll error : ");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}



	return 0;

}
int handle_client(struct epoll_event* ev)
{
	size_t lenr = 0;
	char *buff = 0;

	if (ev->events & EPOLLIN)
	{


		read(ev->data.fd,&lenr,sizeof(size_t));
		buff=malloc(lenr*sizeof(char));
		read(ev->data.fd,buff,lenr);
		free(buff);

		char *msg = "Mam to";
		size_t lenw = strlen(msg);
		write(ev->data.fd,&lenw,sizeof(size_t));
		write(ev->data.fd,msg,lenw);

	}
	return 0;
}

void handle_message(char *msg)
{


}

void handle_login(int fd, char *msg)
{


}

void handle_userlist(int fd, char *msg)
{


}

int init_server(int PORT, int CLIENTS)
{

	memset(&e, 0, sizeof(e));

	//=======================================
	signal(SIGINT, sig_handler);
	signal(SIGTSTP, sig_handler);
	//=======================================

	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr));

	srv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

	if (srv_fd < 0) {
		printf("Cannot create socket\n");
		return 1;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(PORT);


	if (bind(srv_fd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
		printf("Cannot bind socket\n");
		close(srv_fd);
		return 1;
	}

	if (listen(srv_fd, CLIENTS) < 0) {
		printf("Cannot listen\n");
		close(srv_fd);
		return 1;
	}

	epoll_fd = epoll_create(CLIENTS);
	if (epoll_fd < 0) {
		printf("Cannot create epoll\n");
		close(srv_fd);
		return 1;
	}

	e.events = EPOLLIN;
	e.data.fd = srv_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, srv_fd, &e) < 0) {
		printf("Cannot add server socket to epoll\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}
	return 0;
}

/*int add_user(int fd, char* name)
{
	size_t i=0;
	user* u = find_user_by_fd(fd);
	if(u)
		return 1;
	for(;i<CLIENTS;++i)
		if(users[i] == 0)break;

	if(i<CLIENTS)
	{
		users[i]=malloc(sizeof(user));
		users[i]->fd = fd;
		users[i]->name = name;
		return 0;
	}
	return 1;
}
 */
int main(int argc, const char *argv[])
{
	int PORT = atoi(argv[1]);
	//int CLIENTS = 256;//atoi(argv[2]);

	int i = 0;

	struct epoll_event es[CLIENTS];

	if(init_server(PORT,CLIENTS) != 0)
		return 1;

	for(;;) {
		i = epoll_wait(epoll_fd, es, CLIENTS, -1);
		if (i < 0) {
			printf("Cannot wait for events\n");
			close(epoll_fd);
			close(srv_fd);
			return 1;
		}

		for (--i; i > -1; --i) {
			if (es[i].data.fd == srv_fd)
			{
				handle_server(PORT);
			}else
				handle_client(&es[i]);

		}
	}


	return 0;
}



