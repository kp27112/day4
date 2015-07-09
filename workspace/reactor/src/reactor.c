#include "reactor.h"

struct reactor ctx{

};

void event_loop(reactor* self){
	epoll_event events[SIZE
};


int events_cnt = 0;
int i = 0;

while (RUN){
	events_cnt = epoll_wait(self->ctx->epoll_fd, events, SIZE, 1);

	for(i=0; i<events_cnt; ++i){
		event_handler* eh = find_eh(self, events[i].data.fd);
		if(eh)
			eh->handle_event(eh, events[i].events);
	}

}
