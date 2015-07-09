#include "server_eh.h"

struct eh_ctx{
	reactor* r;
	int server_fd;
};

event_handler* create_seh(reactor *r)
{
	//alloc mem eh

	//alloc mem for ctx
	//init server
	//fill contex
	//fill meth
};

static int handle_event(event_handler* self, int32_t){

	//self->ctx->server_fd;
	//reg in reactor self->ctx->r

}

static int get_fd(event_handler* self){
	return self->ctx->server_fd;

}
