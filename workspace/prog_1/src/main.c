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
void clear_client(int fd);
int send_ack(int fd, char reason, char *error);
int send_message(int fd,char *msg,int msg_len);
int init_server();
int handle_server();
int handle_client(struct epoll_event* e);
int rm_user_by_fd(int fd);
user* find_user_by_fd(int fd, size_t* idx);

int main(int argc, const char *argv[])
{
	int PORT = atoi(argv[1]);
	//int CLIENTS = 256;//atoi(argv[2]);

	int i = 0;

	struct epoll_event es[CLIENTS];

	memset(users, 0, CLIENTS * sizeof(user*));

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
			if (es[i].data.fd == srv_fd) {
				if (handle_server() != 0)
					return 1;
			} else {
				handle_client(&es[i]);
			}
		}
	}

	return 0;
}

int init_server(int PORT, int CLIENTS)
{
	//=======================================
	//signal(SIGINT, sig_handler);
	//signal(SIGTSTP, sig_handler);
	//=======================================

	struct sockaddr_in srv_addr;

	memset(&srv_addr, 0, sizeof(srv_addr));
	memset(&e, 0, sizeof(e));

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

void sig_handler(int signo)
{
	if(epoll_fd)
		close(epoll_fd);
	if(srv_fd)
		close(srv_fd);
}

int handle_server()
{
	int cli_fd = -1;
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len = sizeof(cli_addr);

	memset(&cli_addr, 0, sizeof(cli_addr));
	cli_fd = accept(srv_fd, (struct sockaddr*) &cli_addr, &cli_addr_len);
	if (cli_fd < 0) {
		printf("ERROR: Cannot accept client\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	e.data.fd = cli_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_fd, &e) < 0) {
		printf("ERROR: Cannot accept client\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	printf("TRACE: client added with fd: %d\n", cli_fd);

	return 0;

}
int handle_client(struct epoll_event* ev)
{
	size_t msg_len = 0;
	ssize_t read_result = 0;
	char* buff = 0;
	if ( (ev->events & EPOLLERR) || (ev->events & EPOLLRDHUP) || (ev->events & EPOLLHUP) ) { //something really bad happened on cli socket see: man epoll_ctl
		printf("ERROR: Bad event on client socket!\n");
		clear_client(ev->data.fd);
	} else if (ev->events & EPOLLIN) {
		read_result = read(ev->data.fd, &msg_len, sizeof(size_t));
		if ((read_result == sizeof(size_t)) && (msg_len > 0)) {
			printf("TRACE: Real client message len: %zu\n", msg_len);
			buff = malloc((msg_len + 1) * sizeof(char));
			buff[msg_len] = 0;
			read_result = read(ev->data.fd, buff, msg_len);
			if (read_result == msg_len) {
				printf("TRACE: Message received \"%s\"\n", buff);
				handle_message(ev->data.fd, buff);
			} else {
				printf("ERROR: Cannot read exact message!\n");
				clear_client(ev->data.fd);
			}
			free(buff);
		} else {
			printf("ERROR: Client message len is wrong, read res: %zu, msg len: %zu 0!\n", read_result, msg_len);
			clear_client(ev->data.fd);
		}
	} else {
		printf("ERROR: Unexpected event on client socket!\n");
		clear_client(ev->data.fd);
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
		rm_user_by_fd(fd);
	}
}

void handle_userlist(int fd)
{
	char* m = 0;
	size_t m_len = 0;
	size_t m_offset = 0;

	size_t users_cnt = 0;
	size_t user_nicks_len = 0;
	size_t i = 0;
	size_t nick_len = 0;

	//calculate users amount and theirs nicks total length
	for (; i < CLIENTS; ++i) {
		if (users[i]) {
			++users_cnt;
			user_nicks_len += strlen(users[i]->name);
		}
	}

	//calculate total msg len and alloc memory for it
	m_len = user_nicks_len + users_cnt + 1; //e.g. "7.roman.stachu"
	m = malloc((m_len + 1) * sizeof(char));

	//fill msg
	m[0] = '7';
	m_offset = 1;
	for (i=0; i < CLIENTS; ++i) {
		if (users[i]) {
			nick_len = strlen(users[i]->name);
			m[m_offset++] = '.'; //add a dot on current offset and shift offset by one
			strncpy(m + m_offset, users[i]->name, nick_len);
			m_offset += nick_len; //shift offset by nick length
		}
	}

	m[m_len] = 0;

	if (send_message(fd, m, m_len) != 0) {
		rm_user_by_fd(fd);
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

int rm_user_by_fd(int fd)
{
	size_t idx = 0;
	user* u = find_user_by_fd(fd, &idx);
	if (u) {
		users[idx] = 0;
		free(u->name);
		free(u);
		return 0;
	}

	return 1; //user not found
}

user* find_user_by_fd(int fd, size_t* idx)
{
	size_t i=0;
	for(i=0;i<CLIENTS;i++){
		if(users[i] && users[i]->fd == fd){
			if(idx) *idx = i;
			return users[i];
		}
	}
	return 0;
}


int send_message(int fd, char* msg, int len)
{


	write(fd, &len, sizeof(char));
	printf("TRACE: write a message \"%s\"!\n", msg);
	write(fd, msg, len);

	return 0;
}

int send_ack(int fd, char reason, char *error)
{
	int len = 0;
	char *m = 0;
	int result = -1;

	if(reason == '0')
	{
		len = 3;
		m = malloc((len+1)*sizeof(size_t));
		m[0]='1';
		m[1]='.';
		m[2]='0';
		m[3]= 0;
	}else{
		len = strlen(error)+4;
		m = malloc((len+1)*sizeof(size_t));
		m[0]='1';
		m[1]='.';
		m[2]=reason;
		m[3]='.';
		strncpy(m+4, error, len -4);
		m[len] = 0;
	}

	result = send_message(fd, m, len);
	free(m);
	return result;

}

void clear_client(int fd)
{
	close(fd);
	e.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &e);
}





