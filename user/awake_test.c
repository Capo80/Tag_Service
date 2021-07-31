#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "../include/parameters.h"

#define tag_get(key, command, permission) 	syscall(134,key,command,permission)
#define tag_ctl(tag, command)				syscall(183,tag,command)
#define tag_send(tag, level, buffer, size)	syscall(174,tag,level,buffer,size)
#define tag_receive(tag, level, buffer, size) syscall(182,tag,level,buffer,size)

#define THREAD_NUMB 2

void* listener(void* td){

	char* receive = malloc(20);

	int recved = tag_receive(*((int*) td), 1, receive, 20);

	if (recved > 0)
		return (void*)-1;
	else if (recved == 0)
		return (void*)0;
	else
		return (void*)-1;
	

}

void main() {

	int td = tag_get(42, IPC_PUBLIC, T_SHARED);

	pthread_t tids[THREAD_NUMB];
	for (int i = 0; i < THREAD_NUMB; i++)
		pthread_create(tids + i, NULL, listener, &td);

	sleep(1);
	tag_ctl(td, AWAKE_ALL);

	for (int i = 0; i < THREAD_NUMB; i++) {
		int ex;
		pthread_join(tids[i], (void*)&ex);
		if (ex != 0) {
			printf("%-25s test failed\n", "AWAKE TEST:");
			return;
		}
	}

	printf("%-25s test succeded\n", "AWAKE TEST:");

	tag_ctl(td, REMOVE);
}