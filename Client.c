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

#include "UI_library/UI_library.h"
#include "ClientLib.h"

//gcc Client.c -o Client UI_library/UI_library.c ClientLib.c -lSDL2 -lSDL2_image

useconds_t minTimeBetweenPlays = 500000; //microsecs



int main(int argc, char ** argv){				
    signal(SIGINT, ctrl_c_func); //ctrl-c handler
	char colour = argv[3][0];
	if(argc != 4){
		printf("Please initiate program with: ./Client IP PORT COLOR, with COLOUR being one of R,G,B\n");
		exit(-1);
	}

	struct sigaction sa;

    sa.sa_handler = readFromSharedMemory;      // Setup the sighub handler
    sa.sa_flags = SA_RESTART;           // Restart the system call, if at all possible
    sigfillset(&sa.sa_mask);           // Block every signal during the handler

    sigaction(SIGALRM, &sa, NULL);

	int ownColour[3];
	GenerateOwnColour(colour, &ownColour[0], &ownColour[1], &ownColour[2]);

	printf("Client colour r[%d], g[%d], b[%d]\n", ownColour[0], ownColour[1], ownColour[2]);

	struct sockaddr_in server_addr;		
	//socklen_t size_addr;
    server_addr.sin_family = AF_INET;
	SocketSetupAndConect(&server_addr, argv);
	
    
	//Sending own color to server
	send(sock_fd, ownColour, sizeof(ownColour), 0);

	key = ftok("/tmp", 'S');
	size = sizeof(sharedScore *) * MAXCLIENTS;

	if ((shmid = shmget(key, size, 0666)) == -1) {
		perror("shmget");
		exit(1);
	}

	Score = (sharedScore *) shmat(shmid, NULL, 0);
	if (Score == (sharedScore *)(-1)) {
		perror("shmat");
		exit(1);
	}

	//Retrieving client number, initial character position and number of clients/fruits/bricks on board. 
	messageInit msgInit;
	int nB = recv(sock_fd, &msgInit, sizeof(msgInit), 0);
	if(nB <= 0){	
		perror("read Init: ");
		ctrl_c_func();
    }
	else if(nB == 0){
		printf("Server is FULL!!! Please be patient, and try again later :)\n");
		ctrl_c_func();
	}
	//printf("Received initial message %d %d %d \n", msgInit.boardSize_x, msgInit.boardSize_y, msgInit.clientNum);
	clientNum = msgInit.clientNum;
	clientPacPos[0] = msgInit.pac[0];
	clientPacPos[1] = msgInit.pac[1];
	clientMoobPos[0] = msgInit.moob[0];
	clientMoobPos[1] = msgInit.moob[1];
	MAXCLIENTS = msgInit.MAXCLIENTS;
	alarm(msgInit.timeToShowScore);

	
	//Creating board and painting own pac and moob
    create_board_window(msgInit.boardSize_x, msgInit.boardSize_y);
	//printf("Board window created \n");

    SDL_Event event;			

    Event_Show =  SDL_RegisterEvents(1);	//Creates an User-type event 

	//Thread to handle received positions from server
    pthread_t thread_id;
	pthread_create(&thread_id, NULL, threadShow, NULL);
	//printf("Thread Created\n");

    int done = 0;
    
    while (!done){
		while (SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT){
				done = SDL_TRUE;
			}			

			// if the event is of type Event_Show
			else if(event.type == Event_Show){
				

				ServMsg *newMove = event.user.data1;

				//printf("MAIN received: New[%d][%d] Old[%d][%d], type %c, colateral %d, ColNew[%d][%d], ColOld[%d][%d], ColType %c\n", x_new, y_new, x_old, y_old, type, colateralDamage, x_colateral, y_colateral, colateral_x_old, colateral_y_old, colateralType);

				
				//printf("Malloc freed\n");


				if((newMove->type == 'P') || (newMove->type == 'M') || (newMove->type == 'S')){
                    clear_place(newMove->x_old, newMove->y_old);  

                    if(newMove->colateralDamage == 1){
                        //printf("Colateral Part\n");
                        clear_place(newMove->colateral_x_old, newMove->colateral_y_old);
                        if(newMove->colateralType == 'P'){
                            paint_pacman(newMove->x_colateral, newMove->y_colateral, 
								newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], newMove->colateralPaintColour[2]);
                        }
                        else if(newMove->colateralType == 'M'){
                            paint_monster(newMove->x_colateral, newMove->y_colateral, 
								newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], newMove->colateralPaintColour[2]);
                        }
                        else if(newMove->colateralType == 'S'){
                            paint_powerpacman(newMove->x_colateral, newMove->y_colateral, 
								newMove->colateralPaintColour[0], newMove->colateralPaintColour[1], newMove->colateralPaintColour[2]);
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
				else if(newMove->type == 'N'){
					if(newMove->colateralType == 'B'){
						printf("Painting brick\n");
						paint_brick(newMove->x_new, newMove->y_new);
					}
					else {
						
						printf("Painting characters\n");
						if(newMove->colateralType == 'S'){
							paint_powerpacman(newMove->x_new, newMove->y_new, newMove->paintColour[0], 
								newMove->paintColour[1], newMove->paintColour[2]);
						}
						else{
							paint_pacman(newMove->x_new, newMove->y_new, 
								newMove->paintColour[0], newMove->paintColour[1], newMove->paintColour[2]);
						}
						paint_monster(newMove->x_colateral, newMove->y_colateral, 
							newMove->paintColour[0], newMove->paintColour[1], newMove->paintColour[2]);
						
					}
				}
				else if(newMove->type == 'F'){   //New Fruits
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
				else if(newMove->type == 'Q'){
					clear_place(newMove->x_old, newMove->y_old);
					clear_place(newMove->colateral_x_old, newMove->colateral_y_old);
				}

			free(newMove);

				
			}
			//if the event is of type mousemotion
            else if(event.type == SDL_MOUSEMOTION){	//FALTA GARANTIR QUE MEIO SEGUNDO PASSA ENTRE EVENTOS
				message msg;
				int x, y;
				msg.num = clientNum;
				
				int actionMade = 0;
				
				get_board_place(event.motion.x, event.motion.y, &x, &y); //return the place wher the mouse cursor is
				msg.x = clientPacPos[0];
				msg.y = clientPacPos[1];
				if((x != clientPacPos[0]) || (y != clientPacPos[1])){
					if((x > msg.x) && (y == msg.y)){
						actionMade = 1;
						msg.x += 1;
					}
					else if((x < msg.x) && (y == msg.y)){
						actionMade = 1;
						msg.x -= 1;
					}
					if((y > msg.y) && (x == msg.x)){
						actionMade = 1;
						msg.y += 1;
					}
					else if((y < msg.y) && (x == msg.x)){
						actionMade = 1;
						msg.y -= 1;
					}
					msg.type = 'P';
					
					if( ((get_time_usecs() - lastPacPlay) >= minTimeBetweenPlays) && (actionMade == 1)){
						send(sock_fd, &msg, sizeof(msg), 0);
						lastPacPlay = get_time_usecs();
					}
				}	

			}
			else if(event.type == SDL_KEYDOWN){
				if((event.key.keysym.sym == SDLK_RIGHT) || (event.key.keysym.sym == SDLK_UP) ||
					(event.key.keysym.sym == SDLK_DOWN) || (event.key.keysym.sym == SDLK_LEFT) || 
					(event.key.keysym.sym == SDLK_w) || (event.key.keysym.sym == SDLK_a) || 
					(event.key.keysym.sym == SDLK_s) || (event.key.keysym.sym == SDLK_d)){
						message msg;
						msg.type = 'M';
						msg.num = clientNum;

						//this fucntion return the place cwher the mouse cursor is
						msg.x = clientMoobPos[0];
						msg.y = clientMoobPos[1];

						if((event.key.keysym.sym == SDLK_RIGHT) || (event.key.keysym.sym == SDLK_d))
							msg.x ++;
						else if((event.key.keysym.sym == SDLK_LEFT) || (event.key.keysym.sym == SDLK_a))
							msg.x --;
						else if((event.key.keysym.sym == SDLK_UP) || (event.key.keysym.sym == SDLK_w))
							msg.y --;
						else if((event.key.keysym.sym == SDLK_DOWN) || (event.key.keysym.sym == SDLK_s))
							msg.y ++;

						if((get_time_usecs() - lastMoobPlay) >= minTimeBetweenPlays){
							send(sock_fd, &msg, sizeof(msg), 0);
							lastMoobPlay = get_time_usecs();
						}
					}
				
				
			}
		}
	}
    close(sock_fd);
    printf("fim/n");
    exit(0);

}