#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define BUFF_SIZE 1024
#define MAX_USERS 100

int srv_fd = -1;
int epoll_fd = -1;
struct epoll_event e;

typedef struct user {
	int fd;
	char* nick;
} user;

user* users[MAX_USERS];

int add_user(int fd, char* nick);
int rm_user_by_fd(int fd);
int rm_user_by_nick(const char* nick);
user* find_user_by_fd(int fd, size_t* idx);
user* find_user_by_nick(const char* nick, size_t* idx);

int init_server();
int handle_server();
int handle_client(struct epoll_event* e);
void clear_client(int fd);
void handle_message(int fd, char* msg);
void handle_login(int fd, char* msg);
void handle_userlist(int fd);
int send_ack(int fd, char reason, const char* error);
int send_msg(int fd, const char* msg, size_t len);

int main(int argc, const char *argv[])
{
    int i = 0;
    struct epoll_event es[2];

	memset(users, 0, MAX_USERS * sizeof(user*));

	if (init_server() != 0)
		return 1;

    for(;;) {
        i = epoll_wait(epoll_fd, es, 2, -1);
        if (i < 0) {
            printf("ERROR: Cannot wait for events\n");
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

int init_server()
{
	struct sockaddr_in srv_addr;

	memset(&srv_addr, 0, sizeof(srv_addr));
	memset(&e, 0, sizeof(e));

	srv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (srv_fd < 0) {
		printf("ERROR: Cannot create socket\n");
		return 1;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(5556);
	if (bind(srv_fd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
		printf("ERROR: Cannot bind socket\n");
		close(srv_fd);
		return 1;
	}

	if (listen(srv_fd, 1) < 0) {
		printf("ERROR: Cannot listen\n");
		close(srv_fd);
		return 1;
	}

	epoll_fd = epoll_create(2);
	if (epoll_fd < 0) {
		printf("ERROR: Cannot create epoll\n");
		close(srv_fd);
		return 1;
	}

	e.events = EPOLLIN;
	e.data.fd = srv_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, srv_fd, &e) < 0) {
		printf("ERROR: Cannot add server socket to epoll\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	return 0;
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

void clear_client(int fd)
{
	printf("TRACE: clear client with fd: %d\n", fd);
	close(fd);
	e.data.fd = fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &e);
}

void handle_message(int fd, char* msg)
{
	switch (msg[0]) {
		case '1':
			//ACK - do nothing
			break;
		case '2':
			//LogIn
			handle_login(fd, msg);
			break;
		case '6':
			//UserList
			handle_userlist(fd);
			break;
		default:
			//unknown message - just ignore
			break;
	}
}

void handle_login(int fd, char* msg)
{
	// msg shall be NULL terminated string here! see handle_message() line: buff[msg_len] = 0;
	char* nick = malloc((strlen(msg) - 1) * sizeof(char)); //e.g "2.roman" strlen=7 so I need 6 (one extra for NULL)
	strcpy(nick, msg+2); //msg+2 - shift message pointer by two chars to obtain pointer to first letter of nick
	printf("TRACE: adding nick: \"%s\"\n", nick);
	if (add_user(fd, nick) == 0) { //try to add user to storage users
		if (send_ack(fd, '0', 0)) { //user added send ack
			printf("ERROR: Cannot send ack!\n");
			rm_user_by_fd(fd); //cannot send ack so: remove user
			clear_client(fd); //					 clear its connection
		}
	} else {
		if (send_ack(fd, '1', "Cannot add user, server full or nick already exists") != 0) { //send nack
			printf("ERROR: Cannot send nack!\n");
			clear_client(fd); //client was not added so just clear its connection
		}
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
	for (; i < MAX_USERS; ++i) {
		if (users[i]) {
			++users_cnt;
			user_nicks_len += strlen(users[i]->nick);
		}
	}

	//calculate total msg len and alloc memory for it
	m_len = user_nicks_len + users_cnt + 1; //e.g. "7.roman.stachu"
	m = malloc((m_len + 1) * sizeof(char));

	//fill msg
	m[0] = '7';
	m_offset = 1;
	for (i=0; i < MAX_USERS; ++i) {
		if (users[i]) {
			nick_len = strlen(users[i]->nick);
			m[m_offset++] = '.'; //add a dot on current offset and shift offset by one
			strncpy(m + m_offset, users[i]->nick, nick_len);
			m_offset += nick_len; //shift offset by nick length
		}
	}

	m[m_len] = 0;

	if (send_msg(fd, m, m_len) != 0) {
		rm_user_by_fd(fd);
		clear_client(fd);
	}
}

int send_ack(int fd, char reason, const char* error)
{
	size_t len = 0;
	char* m = 0;
	int result = -1;

	if (reason == '0') {
		len = 3;
		m = malloc((len + 1) * sizeof(char));
		m[0] = '1';
		m[1] = '.';
		m[2] = '0';
		m[3] = 0;
	} else {
		len = strlen(error) + 4; //eg. "1.1.3.some failure" four exstra chars for: message type, reason and separators
		m = malloc((len + 1) * sizeof(char));
		m[0] = 1;
		m[1] = '.';
		m[2] = reason;
		m[3] = '.';
		strncpy(m + 4, error, len - 4);
		m[len] = 0;
	}

	result = send_msg(fd, m, len);
	free(m);

	return result;
}

int send_msg(int fd, const char* msg, size_t len)
{
	printf("TRACE: write a message \"%s\" with len: %zu!\n", msg, len);

	if (write(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
		printf("ERROR: Cannot write length!\n");
		return 1;
	}

	if (write(fd, msg, len) != len) {
		printf("ERROR: Cannot write exact message!\n");
		return 1;
	}

	return 0;
}

int add_user(int fd, char* nick)
{
	size_t i = 0;
	user* u = find_user_by_fd(fd, 0);
	if (u)
		return 1; //user already exists

	for (; i < MAX_USERS; ++i)
		if (users[i] == 0) break;

	if (i < MAX_USERS) {
		users[i] = malloc(sizeof(user));
		users[i]->fd = fd;
		users[i]->nick = nick;
		return 0;
	}

	return 1; //there is no place to add user
}

int rm_user_by_fd(int fd)
{
	size_t idx = 0;
	user* u = find_user_by_fd(fd, &idx);
	if (u) {
		users[idx] = 0;
		free(u->nick);
		free(u);
		return 0;
	}

	return 1; //user not found
}

int rm_user_by_nick(const char* nick)
{
	size_t idx = 0;
	user* u = find_user_by_nick(nick, &idx);
	if (u) {
		users[idx] = 0;
		free(u->nick);
		free(u);
		return 0;
	}

	return 1; //user not found
}

user* find_user_by_fd(int fd, size_t* idx)
{
	size_t i = 0;
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i] && users[i]->fd == fd) {
			if (idx) *idx = i;
			return users[i];
		}
	}

	return 0; //user not found
}

user* find_user_by_nick(const char* nick, size_t* idx)
{
	size_t i = 0;
	for (i = 0; i < MAX_USERS; i++) {
		if (users[i] && strcmp(users[i]->nick, nick) == 0) {
			if (idx) *idx = i;
			return users[i];
		}
	}

	return 0; //user not found
}

