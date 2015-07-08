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
void handle_message(int fd, char *msg);
void handle_login(int fd, char *msg);
void handle_userlist(int fd);
int send_message(int fd,char *msg,int msg_len);
void clear_client(int fd);
int send_ack(int fd, char reason, char *error);
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
		buff=malloc(lenr*sizeof(char)+1);
		read(ev->data.fd,buff,lenr);
		if(lenr <= 0)
			return 1;
		else{
			handle_message(ev->data.fd,buff);
			free(buff);
		}
	}
	return 0;
}

void handle_message(int fd, char *msg)
{

	switch(msg[0])
	{
	case '2' :
		handle_login(fd, msg);
		break;
	case '6' :
		handle_userlist(fd);
		break;
	case 'q':
		clear_client(fd);
		if(epoll_fd)
			close(epoll_fd);
		if(srv_fd)
			close(srv_fd);
		break;
	default:
		clear_client(fd);
		if(epoll_fd)
			close(epoll_fd);
		break;

	}
}
void handle_login(int fd, char *msg)
{
	char* name = malloc((strlen(msg)-1)*sizeof(char));
	strcpy(name, msg+2);
	if(add_user(fd,name) == 0 ){
		send_ack(fd, '0', 0);
	} else {
		send_ack(fd, '1', "Error - server is full or name already exist");
		clear_client(fd);
	}
}

int add_user(int fd, char* name)
{
	size_t i=0;

	for(;i<CLIENTS;++i)
		if (users[i] && (strcmp(users[i]->name,name) == 0) )
			return 1;

	i=0;
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

user* find_user_by_id(int fd, size_t* idx)
{
	size_t i=0;
	for(i=0;i<CLIENTS;i++){
		if(users[i] && users[i]->fd == fd){
			if(idx) *idx = i;
			return users[i];
		}
	}
}

void handle_userlist(int fd)
{
	char* m = 0;
	size_t m_l = 0;
	size_t i = 0;
	size_t all_name_l = 0;
	size_t name_l =0;
	size_t msg_l = 0;
	size_t user_c = 0;
	size_t m_offset =0;

	for(;i<CLIENTS;++i)
	{
		if(users[i]){
			++user_c;
			all_name_l += strlen(users[i]->name);
		}

	}

	m_l = all_name_l + user_c + 1;
	m = malloc((msg_l+1)*sizeof(char));

	m[0]='7';
	m_offset = 1;
	for(i=0;i<CLIENTS;++i)
	{
		if(users[i]){
			name_l = strlen(users[i]->name);
			m[m_offset++] = '.';
			strncpy(m + m_offset, users[i]->name, name_l);
			m_offset += name_l;
		}
	}

	m[m_l] = 0;

	if(send_message(fd, m, m_l) != 0)
		clear_client(fd);


}
int send_message(int fd, char* msg, int msg_len)
{
	if(msg_len>0){
		write(fd,&msg_len,sizeof(char));
		write(fd,msg,msg_len);
		return 0;
	}
	return 1;
}

int send_ack(int fd, char reason, char *error)
{
	int len = 0;
	char *m = 0;
	if(reason == '0')
	{
		len = 4;
		m = malloc(len*sizeof(size_t));
		m[0]='1';
		m[1]='.';
		m[2]=reason;
	}else{
		len = strlen(error)+5;
		m = malloc(len*sizeof(size_t));
		m[0]='1';
		m[1]='.';
		m[2]=reason;
		m[3]='.';
		strcpy(m+4, error);

	}
	printf("%s", m);
	if(send_message(fd, m, len) == 0)
		return 0;
	else
		return 1;
}

void clear_client(int fd)
{
	close(fd);
	close(srv_fd);

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



