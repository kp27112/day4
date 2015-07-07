
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>


void *foo1(void* a)
{
		pid_t p = fork();
       	sleep(1);
return 0;
}


int main(void)
{
    pthread_t t_id1;
    pthread_create(&t_id1,NULL,foo1,NULL);
	printf ("lol");
    pid_t p = fork();

   // pthread_join(t_id,0);

return 0;
}
