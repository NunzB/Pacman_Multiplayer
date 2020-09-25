#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include <sys/time.h>
#include <pthread.h>

#include "ClientLib.h"

void GenerateOwnColour(char colour, int *c0, int *c1, int *c2){
	srand(time(NULL));

	if((colour == 'r') || (colour == 'R')){
		*c0 = (rand()%150)+105;
		*c1 = rand()%80;
		*c2 = rand()%80;
	} 
	else if((colour == 'g') || (colour == 'G')){
		*c0 = rand()%80;
		*c1 = (rand()%150) + 105;
		*c2 = rand()%80;
 	}
	else if((colour == 'b') || (colour == 'B')){
		*c0 = rand()%80;
		*c1 = rand()%80;
		*c2 = (rand()%150) + 105;
 	}
	else{
		printf("Please insert a valid colour R, G or B.\n");
		exit(-1);
	}
	return;
}

void SocketSetupAndConect(struct sockaddr_in *server_addr, char ** argv){
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1){
		perror("Creating socket: ");
		exit(-1);
	}

    server_addr->sin_port = htons(atoi(argv[2]));
    if(inet_aton(argv[1], &(server_addr->sin_addr)) == 0){
        fprintf(stderr, "Invalid address\n");
        exit(EXIT_FAILURE);
    }

    if(connect(sock_fd, (const struct sockaddr *)server_addr, sizeof(*server_addr)) == -1){
        printf("Error connecting\n");
        ctrl_c_func();
    }

	return;
}

void * threadShow(void * arg){
	//int activeObj = *((int*)arg);
	SDL_Event new_event;
    
    int nBytes;
	//int mainRead = 1;
	//printf("Size of Server msg %d", sizeof(Smsg));

	while(1){
		ServMsg * Smsg = malloc(sizeof(ServMsg));
		
		nBytes = recv(sock_fd, Smsg, sizeof(ServMsg), 0);
        if(nBytes < 0){
            perror("read: ");
            ctrl_c_func();
        }
		else if(nBytes == 0){
			printf("Exiting... Server has left\n");
			ctrl_c_func();
		}
		else{	
			printf("Received [%d][%d] type %c; ", Smsg->x_new, Smsg->y_new, Smsg->type);
			printf("Colateral[%d][%d] type %c, at %ld\n", Smsg->x_colateral, Smsg->y_colateral, Smsg->colateralType, time(NULL));
			usleep(500);
			// clear the event data
			SDL_zero(new_event);
			//getchar();
			// define event type
			new_event.type = Event_Show;
			//assign the event data
			new_event.user.data1 = Smsg;
			//new_event.user.data2 = &mainRead;
			// send the event
			//mainRead = 0;
			SDL_PushEvent(&new_event);

			if(Smsg->num == clientNum){
				if((Smsg->type == 'P') || (Smsg->type == 'S')){
					clientPacPos[0] = Smsg->x_new;
					clientPacPos[1] = Smsg->y_new;
				}
				else if(Smsg->type == 'M'){
					clientMoobPos[0] = Smsg->x_new;
					clientMoobPos[1] = Smsg->y_new;
				}
			}
			if((Smsg->colateralNum == clientNum) && (Smsg->colateralDamage == 1)){
				if((Smsg->colateralType == 'P') || (Smsg->colateralType == 'S')){
					clientPacPos[0] = Smsg->x_colateral;
					clientPacPos[1] = Smsg->y_colateral;
				}
				else if(Smsg->colateralType == 'M'){
					clientMoobPos[0] = Smsg->x_colateral;
					clientMoobPos[1] = Smsg->y_colateral;
				}
			}
			
			
			//getchar();
			//fflush(stdin);
		}
	}
}

void readFromSharedMemory(){
    
	printf("SCORE BOARD:\n");
    for (int i = 0; i < MAXCLIENTS; i++) {
		
		if(Score[i].activeClient == 1){
            printf("Client [%d] -> SuperPac Eats: %d; Monster Eats: %d; Fruits Eaten: %d\n", i, Score[i].superPacEaten, 
					Score[i].monsterEaten, Score[i].fruitEaten);
		}
	}
	alarm(60);
    
	return;
}

void ctrl_c_func(){
	close(sock_fd);
    //unlink(SOCK_ADDRESS);
    if (shmdt(Score) == -1) {
            perror("ctrl_c shmdt");
            exit(1);
    }
    
    printf("Closing and unlinking sockets\n"); 
    exit(0);
}

//Gets time now in usec
long int get_time_usecs(){	
    double t1;
    struct timeval ts;

    gettimeofday(&ts, NULL);  

    t1 = ts.tv_sec*1.0e6 + ts.tv_usec;  

    return t1;
}