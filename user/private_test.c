#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "../include/parameters.h"

#define tag_get(key, command, permission) 	syscall(134,key,command,permission)
#define tag_ctl(tag, command)				syscall(183,tag,command)
#define tag_send(tag, level, buffer, size)	syscall(174,tag,level,buffer,size)
#define tag_receive(tag, level, buffer, size) syscall(182,tag,level,buffer,size)

int main(int argc, char** argv){

	int td1, td2;

	td1 = tag_get(42, IPC_PRIVATE, T_SHARED);
	td2 = tag_get(42, IPC_PUBLIC, T_SHARED);

	if (td1 != -1 && td2 == -1)
		printf("%-25s test succeded\n", "PRIVATE:");
	else
		printf("%-25s test failed\n", "PRIVATE:");
	
	tag_ctl(td1, REMOVE);
	return 1;

}