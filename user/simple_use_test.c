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
#define audit(param)						syscall(214, param)

sem_t ready_sem;

void* listener(void* td){

	char* receive = malloc(20);

	sem_post(&ready_sem);
	int recved = tag_receive(*((int*) td), 1, receive, 20);

	if (recved > 0)
		return (void*)0;
	else if (recved == 0)
		return (void*)-1;
	else
		return (void*)-1;
	

}
void* writer(void* td){


	char* send = "ciao";

	if (tag_send(*((int*) td), 1, send, 4) < 0){
		return (void*)-1;
	}

	return (void*)0;
}


int main(int argc, char** argv){

	//audit(42);
	//return 1;

	sem_init(&ready_sem, 0, 0);

	int td = tag_get(42, IPC_PUBLIC, T_SHARED);
	
	pthread_t t1, t2;
	pthread_create(&t1, NULL, listener, &td);
	sem_wait(&ready_sem);
	pthread_create(&t2, NULL, writer, &td);

	int res1, res2;
	pthread_join(t1, (void*)&res1);
	pthread_join(t2, (void*)&res2);

	tag_ctl(td, REMOVE);
	sem_destroy(&ready_sem);

	if (res1 == 0 && res2 == 0)
		printf("%-25s test succeded\n", "SIMPLE USE:");
	else
		printf("%-25s test failed\n", "SIMPLE USE:");
		
	return 1;

}