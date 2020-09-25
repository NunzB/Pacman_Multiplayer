#ifndef CLIENTLIB_H
#define CLIENTLIB_H

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

int MAXCLIENTS;
int sock_fd;
int clientNum;
int clientPacPos[2], clientMoobPos[2];  //Current client characters position
double lastPacPlay, lastMoobPlay;   //time instance of last play

Uint32 Event_Show;

typedef struct argIntoThread{
  Uint32 eventType;
  Sint32 eventCode;
}argIntoThread;


typedef struct ServMsg{
    int num, colateralNum;
    char type, colateralType;
    int x_new, y_new, x_old, y_old;
    int x_colateral, y_colateral, colateral_x_old, colateral_y_old;
    int colateralDamage;
    int paintColour[3], colateralPaintColour[3];
} ServMsg;

typedef struct messageInit{
	int boardSize_x;
    int boardSize_y;
    int clientNum;
	int pac[2];
    int moob[2];
	int MAXCLIENTS;
    int timeToShowScore;
} messageInit;

typedef struct message{
    int num;
    char type;      //P -> pacman, M -> monster, N -> NewClient, B -> Brick, Q -> CLient quit
    int x, y;
} message;

typedef struct sharedScore{
    int activeClient;
    int num;
    int monsterEaten;
    int superPacEaten;
    int fruitEaten;
} sharedScore;

//struct sharedScore *Score;
char c;
int shmid;
key_t key;
sharedScore * Score ;
int size;


//Functions

void GenerateOwnColour(char colour, int *c0, int *c1, int *c2); //Generate client colour 

void SocketSetupAndConect(struct sockaddr_in *server_addr, char ** argv); //Setup socket and connect to serv

void * threadShow(void * arg); //Thread to receive msg from server 
void ctrl_c_func(); //ctrl_c handler
long int get_time_usecs(); //return current time instance: secs + microsecs;

void readFromSharedMemory(); //Read Score board from shared Memory



#endif