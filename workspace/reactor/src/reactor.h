
typedef struct refactor_ctx reactor_ctx;

typedef struct reactor {
	reactor_ctx ctx;
	void (*register_handler)(reactor_self, event_handler);
	void(*rm_handler)(reactor* self, int fd);
	void(event_loop)(reactor* self);

	return reactor;
};

reactor* create_reactor(size_t size);
