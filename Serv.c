#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <time.h>

#include "UI_library/UI_library.h"
#include "ServLib.h"
#include "ServThreads.h"

//gcc Serv.c -o Serv UI_library/UI_library.c ServThreads.c ServLib.c -lSDL2 -lSDL2_image -lpthread -Wall -Wextra -Werror


int main(int argc, char ** argv){
    signal(SIGINT, ctrl_c_func); //ctrl-c handler 

    if(argc != 3){
        printf("Please input board size( ./Serv X Y )\n");
        exit(-1);
    }
    
    if(sscanf(argv[1], "%d", &BoardSize[0]) == 0){
		printf("usage: board_drawer n_cols n_lines\nn_cols should be an integer\n");
		exit(-1);
	}
	if(sscanf(argv[2], "%d", &BoardSize[1]) == 0){
		printf("usage: board_drawer n_cols n_lines\nn_lines should be an integer\n");
		exit(-1);
	} 

    int numBricks;
    //Creates game board, fill structure to store board info (**Board) and paints bricks 
    create_board_window(BoardSize[0], BoardSize[1]);
    numBricks = GenerateBoardStructure(BoardSize[0], BoardSize[1]);
    for(int i = 0; i < BoardSize[0]; i++){
        for(int j = 0; j < BoardSize[1]; j++){
            if(Board[i][j].type == 'B')
                paint_brick(i, j);
                //printf("[%d][%d] Brick\n", i, j);
        }
    }

    //Definition of max number of players based on board size and nº of bricks
    MAXCLIENTS = (((BoardSize[0]*BoardSize[1]) - numBricks)+2)/4;

    //Allocates Structure to store clients info
    ClientInfo = (struct ClientThreadInfo *) malloc(sizeof(struct ClientThreadInfo)*MAXCLIENTS);
    memset(ClientInfo, 0, sizeof(struct ClientThreadInfo)*MAXCLIENTS);

    //Allocates struct to store score board.
    //Score = (sharedScore *) malloc(sizeof(sharedScore)*MAXCLIENTS);

    //Sets up socket to receive clients
    servSocketSetup();  

    ///Initialyze mutexs, rwLocks, cond vars...
    initializeSyncVars();
    setupSharedMemory();

    //Creates thread to accept new clients and to handle fruits
    pthread_t acceptThreadID, fruitThreadID;
    pthread_create(&acceptThreadID, NULL, threadAccept, NULL);
    pthread_create(&fruitThreadID, NULL, ShowFruitsOnBoard, NULL);

    //Register new user SDL Event
    Event_Show =  SDL_RegisterEvents(1);

    SDL_Event event;
    
    int done = 0;
    while (!done){
        
        /*pthread_mutex_lock();
        pthread_cond_wait();
        pthread_mutex_unlock();*/


		while(SDL_PollEvent(&event)) {
            
			if(event.type == SDL_QUIT){
				done = SDL_TRUE;
			}

			// if the event is of type Event_Show
			else if(event.type == Event_Show){                  
                char interaction = (char)event.user.code;

                // retrieve positions
				ServMsg *newMove = event.user.data1;
                
                //printf("MAIN received: New[%d][%d] Old[%d][%d], type %c, colateral %d, ColNew[%d][%d], ColOld[%d][%d], ColType %c|\n", newMove->x_new, newMove->y_new, newMove->x_old, newMove->y_old, newMove->type, newMove->colateralDamage, newMove->x_colateral, newMove->y_colateral, newMove->colateral_x_old, newMove->colateral_y_old, newMove->colateralType);

                if((newMove->type == 'P') || (newMove->type == 'M') || (newMove->type == 'S')){
                    clear_place(newMove->x_old, newMove->y_old);  

                    if(newMove->colateralDamage == 1){
                        clear_place(newMove->colateral_x_old, newMove->colateral_y_old);

                        if(newMove->colateralType == 'P'){
                            paint_pacman(newMove->x_colateral, newMove->y_colateral, 
                                newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], 
                                newMove->colateralPaintColour[2]);
                        }
                        else if(newMove->colateralType == 'M'){
                            paint_monster(newMove->x_colateral, newMove->y_colateral, 
                                newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], 
                                newMove->colateralPaintColour[2]);
                        }
                        else if(newMove->colateralType == 'S'){
                            paint_powerpacman(newMove->x_colateral, newMove->y_colateral, 
                                newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], 
                                newMove->colateralPaintColour[2]);
                        }
                    }

                    if(newMove->type == 'P'){
                        paint_pacman(newMove->x_new, newMove->y_new, newMove->paintColour[0], 
                            newMove->paintColour[1], newMove->paintColour[2]);
                    }
                    else if(newMove->type == 'M'){
                        paint_monster(newMove->x_new, newMove->y_new, newMove->paintColour[0], 
                            newMove->paintColour[1], newMove->paintColour[2]);
                    }
                    else if(newMove->type == 'S'){
                        paint_powerpacman(newMove->x_new, newMove->y_new, newMove->paintColour[0], 
                            newMove->paintColour[1], newMove->paintColour[2]);
                    }

                }
                //New player
                else if(newMove->type == 'N'){   
                    paint_pacman(newMove->x_new, newMove->y_new, newMove->paintColour[0], 
                        newMove->paintColour[1], newMove->paintColour[2]);
                    paint_monster(newMove->x_colateral, newMove->y_colateral, newMove->paintColour[0], 
                        newMove->paintColour[1], newMove->paintColour[2]);
				}
                //New Fruits
                else if(newMove->type == 'F'){   
                    if(newMove->colateralDamage == 1){
                        paint_cherry(newMove->x_colateral, newMove->y_colateral);
                        if(newMove->colateralType == 'N'){
                            paint_lemon(newMove->x_new, newMove->y_new);
                        }
                    }
                    else{
                        paint_lemon(newMove->x_new, newMove->y_new);
                    }
                    
                }
                //Client quit
				else if(newMove->type == 'Q'){   
					clear_place(newMove->x_old, newMove->y_old);
					clear_place(newMove->colateral_x_old, newMove->colateral_y_old);
				}
                else{
                    printf("Invalid ServMsg type\n");
                }

                //Sending msg to every active client
                for(int i = 0; i < MAXCLIENTS; i++){
                    pthread_rwlock_rdlock(&(SyncVars.rwLockActiveClient));
                    if(ClientInfo[i].clientActive == 1){
                        send(ClientInfo[i].clientfd, newMove, sizeof(ServMsg), 0);
                    }
                    pthread_rwlock_unlock(&(SyncVars.rwLockActiveClient));
                }   
                free(newMove);
                //printf("\n");

            }    
		}
	}
    close(sock_fd);
    //close(dgram_fd);

    printf("Server Ended\n");
    exit(0);

}