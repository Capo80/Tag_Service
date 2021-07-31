#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include "../include/parameters.h"

#define tag_get(key, command, permission) 	syscall(134,key,command,permission)
#define tag_ctl(tag, command)				syscall(183,tag,command)
#define tag_send(tag, level, buffer, size)	syscall(174,tag,level,buffer,size)
#define tag_receive(tag, level, buffer, size) syscall(182,tag,level,buffer,size)

#define THREAD_NUMB 9 //multiple of 3

struct thread_args {

	int td;
	char* buffer;
	size_t size;

};

void* listener(void* args){

	struct thread_args* t_args = (struct thread_args*) args;
	char* receive = malloc(t_args->size);

	int recved = tag_receive(t_args->td, 1, receive, t_args->size);

	if (recved > 0) {
		if (strcmp(receive, t_args->buffer) == 0)
			return (void*)0;
		else
			return (void*) -1;
	}
	else if (recved == 0)
		return (void*)-1;
	else
		return (void*)-1;
	

}
void* writer(void* args){

	struct thread_args* t_args = (struct thread_args*) args;

	if (tag_send(t_args->td, 1, t_args->buffer, t_args->size) < 0){
		return (void*)-1;
	}

	return (void*)0;
}


void main(int argc, char** argv){


	int td = tag_get(42, IPC_PUBLIC, T_SHARED);
	int td2 = tag_get(42 + 42, IPC_PUBLIC, T_SHARED);
	int td3 = tag_get(42 + 42 + 42, IPC_PUBLIC, T_SHARED);
	
	struct thread_args tag1;
	struct thread_args tag2;
	struct thread_args tag3;

	tag1.td = td;
	tag1.buffer = malloc(7);
	tag1.buffer = "Arthur";
	tag1.size = 7;
	tag2.td = td2;
	tag2.buffer = malloc(5);
	tag2.buffer = "Ford";
	tag2.size = 5;
	tag3.td = td3;
	tag3.buffer = malloc(7);
	tag3.buffer = "Zaphod";
	tag3.size = 7;

	pthread_t listen_tids[THREAD_NUMB];
	pthread_t write_tids[THREAD_NUMB];
	for (int i = 0; i < THREAD_NUMB; i += 3){
		pthread_create(listen_tids + i, NULL, listener, &tag1);
		pthread_create(listen_tids + i + 1, NULL, listener, &tag2);	
		pthread_create(listen_tids + i + 2, NULL, listener, &tag3);	
	}
	sleep(1);
	for (int i = 0; i < THREAD_NUMB; i += 3){
		pthread_create(write_tids + i, NULL, writer, &tag1);
		pthread_create(write_tids + i + 1, NULL, writer, &tag2);	
		pthread_create(write_tids + i + 2, NULL, writer, &tag3);	
	}

	for (int i = 0; i < THREAD_NUMB; i++) {
		int ex;
		pthread_join(write_tids[i], (void*)&ex);
		if (ex != 0) {
			printf("%-25s test failed\n", "CONCURRENCY TEST:");
			return;
		}
	}
	for (int i = 0; i < THREAD_NUMB; i++) {
		int ex;
		pthread_join(listen_tids[i], (void*)&ex);
		if (ex != 0) {
			printf("%-25s test failed\n", "CONCURRENCY TEST:");
			return;
		}
	}

	printf("%-25s test succeded\n", "CONCURRENCY TEST:");
	tag_ctl(td, REMOVE);
	tag_ctl(td2, REMOVE);
	tag_ctl(td3, REMOVE);
	

	return;

}