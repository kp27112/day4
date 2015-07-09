
typedef struct eh_ctx eh_ctx;

typedef struct event_handler{
	eh_ctx ctx;
	int (*handle_event) (event_handler* self, int32_t);
	int (*get_fd) (event_handler* self);
}event_handler;


