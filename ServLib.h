#ifndef SERVLIB_H
#define SERVLIB_H

#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/un.h>
#include <SDL2/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

//Variables
int MAXCLIENTS;
int numActiveClients;

long int ScoreBoardInit;


struct BoardInfo{
	int occupied; //0 if unnocupied, 1 if occupied
    char type;  //M monstro, P pac, B tijolo, F fruta
    int ClientNum;  //-1 if Brick, or Fruit
};
struct BoardInfo **Board;

struct ClientThreadInfo{
    int clientActive;           //0 if client has exited, 1 if active
    pthread_t threadID;         //ID of thread treating a certain client
    int clientfd;               //fd to write/read to client
    struct sockaddr_in client_addr;
    struct sockaddr_in dgramClient_addr;
    int x_pac, y_pac;
    int x_moob, y_moob;
    int colour[3];
    double lastPacPlay, lastMoobPlay;     //time instance when last play was accepted by the server 
    int superPowerEats;
    int superPower;         //0 if normal PAc, 1 if super power
};
struct ClientThreadInfo *ClientInfo;

typedef struct argIntoThread{
    int sizeBoard_x;
    int sizeBoard_y;
}argIntoThread;

typedef struct message{ //Messages from client to server
    int num;
    char type;      //P -> pacman, M -> monster, S -> SuperPoweredPac, B -> Brick
	//char subType;	//if type = 'N', subType must be filled accordingly
    int x, y;   //Position to analyse by server
} message;

typedef struct ServMsg{ //Messages from server to client
    int num, colateralNum;
    char type, colateralType;
    int x_new, y_new, x_old, y_old;
    int x_colateral, y_colateral, colateral_x_old, colateral_y_old;
    int colateralDamage;    //0 if no colateralDamage, 1 if so
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

struct SyncVars{
    pthread_rwlock_t rwLockActiveClient;
    pthread_mutex_t muxGameRules;
    pthread_rwlock_t rwLockBoard;
    pthread_rwlock_t rwLockClientInfo;
    pthread_mutex_t muxFruits;
    pthread_cond_t condFruits;
    pthread_cond_t condTimeFruits;
}SyncVars;

int BoardSize[2];

int sock_fd;

struct sockaddr_in server_addr;     //Server address info
Uint32 Event_Show;

//Shared Memory Structure
typedef struct sharedScore{
    int activeClient;
    int num;
    int monsterEaten;
    int superPacEaten;
    int fruitEaten;
} sharedScore;

sharedScore * Score;
key_t key;
int shmid;


//Functions
void ctrl_c_func();
void UpdateOldClientsScreen(int counter, ServMsg * Smsg);
void UpdateNewClientScreen(int boardSize_x, int boardSize_y, int counter);
int GenerateBoardStructure(int size_x, int size_y);
void RetrieveNewClientColour(int counter);
long int get_time_usecs();
int AnalyseClientPlay(message msg, int pos[]);
void GenerateInitialMessage(int counter, messageInit *msgInit);

char ApplyGameRules(message *msg, ServMsg *newMove, int pos_old[]);

void generateValidRandomPos(int* x, int* y);
void servSocketSetup();

void initializeSyncVars();

void paintFruitEvent();
void inactivityJump();

void setupSharedMemory();





#endif