#ifndef SERVTHREADS_H
#define SERVTHREADS_H

#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <SDL2/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>


void * ShowFruitsOnBoard(void * arg);   //Responsable for updating fruits on board

void * ClientThreadShow(void * arg); //Handles connected client

void *threadAccept(void *arg); //Handles and organizes client info when new connect




#endif