#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <SDL2/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <unistd.h>

#include "ServLib.h"


//Creates events, send to main (which paints, and sends to other)
void * ShowFruitsOnBoard(void * arg){
    int numDesiredFruits;
    int numActualFruits = 0;
    int oldNumClients;

    SDL_Event new_event;

    while(1){

        //CondVar signaled when new client entered, old client quit, or fruit eaten
        pthread_mutex_lock(&(SyncVars.muxFruits));
        int ret = pthread_cond_wait(&(SyncVars.condFruits), &(SyncVars.muxFruits));
        if(ret != 0){
            perror("Cond Wait Fruits: ");
        }
        pthread_mutex_unlock(&(SyncVars.muxFruits));

        //Nº of fruits depending on nº of clients
        numDesiredFruits = 2*((numActiveClients) - 1);

        int x1, y1, x2, y2;
        //if new client or old client has quit
        if((numDesiredFruits >= 0) && (numDesiredFruits != numActualFruits)){

            ServMsg *Smsg = (ServMsg *) malloc(sizeof(ServMsg));
            memset(Smsg, 0, sizeof(ServMsg));

            if((numActualFruits < numDesiredFruits)){     //Client joined -> need to paint fruits
                //Generating new 2 fruits position
                srand(2*time(NULL));
                generateValidRandomPos(&x1, &y1);
                generateValidRandomPos(&x2, &y2);
                
                //Writing info on board struct
                pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
                Board[x1][y1].occupied = 1;
                Board[x1][y1].type = 'F';
                Board[x1][y1].ClientNum = -1;

                Board[x2][y2].occupied = 1;
                Board[x2][y2].type = 'F';
                Board[x2][y2].ClientNum = -1;
                pthread_rwlock_unlock(&(SyncVars.rwLockBoard));
            
                Smsg->type = 'F';
                Smsg->x_new = x1;
                Smsg->y_new = y1;
                Smsg->colateralDamage = 1;
                Smsg->colateralType = 'N';
                Smsg->x_colateral = x2;
                Smsg->y_colateral = y2;
            }
            else if((numActualFruits > numDesiredFruits) ){    //Client quit -> need to erase fruits
                int counter = 0;

                //Looking for places to erase fruits
                pthread_rwlock_rdlock(&(SyncVars.rwLockBoard));
                for(int i = 0; i < BoardSize[0]; i++){
                    for(int j = 0; j < BoardSize[1]; j++){
                        if(Board[i][j].type == 'F'){
                            if(counter == 0){
                                x1 = i;
                                y1 = j;
                                counter++;
                            }
                            else{
                                x2 = i;
                                y2 = j;
                            }
                        }
                    }
                }
                pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

                //Writing update to board structure
                pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
                Board[x1][y1].occupied = 0;
                Board[x1][y1].type = ' ';
                Board[x1][y1].ClientNum = -1;

                Board[x2][y2].occupied = 0;
                Board[x2][y2].type = ' ';
                Board[x2][y2].ClientNum = -1;
                pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

                Smsg->type = 'Q';
                Smsg->x_old = x1;
                Smsg->y_old = y1;
                Smsg->colateralDamage = 1;
                Smsg->colateral_x_old = x2;
                Smsg->colateral_y_old = y2;
                
            }
        
            printf("FRUTAS desired %d actual %d \n", numDesiredFruits, numActualFruits);
        
            
            if((numActualFruits != numDesiredFruits)){
                SDL_zero(new_event);
                new_event.type = Event_Show;
                new_event.user.data1 = Smsg;
                SDL_PushEvent(&new_event);
                
                numActualFruits = numDesiredFruits;
            }
        }
        //Caso fruta tenha sido comida
        else if(numDesiredFruits > 0){
            //Defining 2seconds of waiting time
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            ts.tv_nsec = 0;
            
            //waits 2s or until a new fruit is eaten
            pthread_mutex_lock(&(SyncVars.muxFruits));
            pthread_cond_timedwait(&(SyncVars.condFruits), &(SyncVars.muxFruits), (const struct timespec *) &ts);
            pthread_mutex_unlock(&(SyncVars.muxFruits));

            int fruits = 0;
            pthread_rwlock_rdlock(&(SyncVars.rwLockBoard));
            for(int i = 0; i < BoardSize[0]; i++){
                for(int j = 0; j < BoardSize[1]; j++){
                    if((Board[i][j].occupied == 1) && (Board[i][j].type == 'F'))
                        fruits++;
                }
            }
            pthread_rwlock_unlock(&(SyncVars.rwLockBoard));
            int done = 0;
            //printf("\t HAVE %d fruits, NEED %d\n", fruits, numDesiredFruits);
            while(!done){
                if(fruits < numDesiredFruits){
                    paintFruitEvent();
                    fruits++;
                }
                else{
                    done = 1;
                }
            }
            
        }
        

    }
    pthread_exit(0);

}


//Function for each Client thread (Receives msgs, analyse valid play, and send SDL event to main thread)
void * ClientThreadShow(void * arg){
    int *args = (int*)arg;
    int num = *args;
    SDL_Event new_event;

    //Retrieve from socket new client colour (stored at tinfo[counter].colour[0]/g/b)
    RetrieveNewClientColour(num);

    //Generating initial positions and send msg to client
    messageInit msgInit;
    GenerateInitialMessage(num, &msgInit);
    send(ClientInfo[num].clientfd, &msgInit, sizeof(msgInit), 0);
 
    //Sending position  of other objects to the new client
    //printf("Sending other object position to new client\n");
    UpdateNewClientScreen(BoardSize[0], BoardSize[1], num);

    //Sending position of new client to main (that sends to every active client)
    ServMsg *Smsg = (ServMsg *) malloc(sizeof(ServMsg));
    memset(Smsg, 0, sizeof(ServMsg));
    //printf("Sending new client position to other clients\n");
    UpdateOldClientsScreen(num, Smsg);
    new_event.type = Event_Show;
    new_event.user.data1 = Smsg;
    SDL_PushEvent(&new_event);  

    message msg;
    char code;
    int nBytes;

    struct timeval tv;
    tv.tv_sec = 30;
    tv.tv_usec = 0;

    //Defining maximum time thread blocks on recv. if 30s have passed -> inactivity jump
    setsockopt(ClientInfo[num].clientfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv, sizeof(struct timeval));

	while(1){
        nBytes = recv(ClientInfo[num].clientfd, &msg, sizeof(msg), 0);
        printf("Th Client %d: Received play [%d][%d] type: %c, num %d. ", num, msg.x, msg.y, msg.type, msg.num); 

        ServMsg *newMove = (ServMsg *) malloc(sizeof(ServMsg));
        memset(newMove, 0, sizeof(ServMsg));

        //Message received
        if(nBytes > 0)  
        {
            msg.num = num;
            int pos_old[2];

            //Play was valid
            if(AnalyseClientPlay(msg, pos_old) == 1){
                //Stores when play was made
                if(msg.type == 'P'){
                    ClientInfo[num].lastPacPlay = get_time_usecs();
                }
                else if(msg.type == 'M'){
                    ClientInfo[num].lastMoobPlay = get_time_usecs();
                }

                printf("-> Valid play\n");
                code = ApplyGameRules(&msg, newMove, pos_old);

                //If code == W -> This Client has been victim of colateral damage since play was analysed            
                if(code != 'W'){
                    SDL_zero(new_event);                // clear the event data
                    new_event.type = Event_Show;        // define event type
                    new_event.user.code = (int)code;

                    new_event.user.data1 = newMove;

                    SDL_PushEvent(&new_event);          //send the event to SDL queue
                    
                }
            }
            //Play was not valid
            else{
                printf("-> Unvalid play\n");
            }
        }
        //Client has quit
        else if(nBytes == 0)    
        {
            printf("Th Client %d:", num);
            printf("Client number exited\n");

            //Updates activeClient flag
            pthread_rwlock_wrlock(&(SyncVars.rwLockActiveClient));
            ClientInfo[num].clientActive = 0;
            pthread_rwlock_unlock(&(SyncVars.rwLockActiveClient));

            Score[num].activeClient = 0;

            //Updates board positions
            pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
            Board[ClientInfo[num].x_pac][ClientInfo[num].y_pac].occupied = 0;
            Board[ClientInfo[num].x_pac][ClientInfo[num].y_pac].type = ' ';
            Board[ClientInfo[num].x_moob][ClientInfo[num].y_moob].occupied = 0;
            Board[ClientInfo[num].x_moob][ClientInfo[num].y_moob].type = ' ';
            pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

            //Inform main that this client has exited
            newMove->type = 'Q';
            pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
            newMove->x_old = ClientInfo[num].x_pac;
            newMove->y_old = ClientInfo[num].y_pac;
            newMove->colateral_x_old = ClientInfo[num].x_moob;
            newMove->colateral_y_old = ClientInfo[num].y_moob;
            pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));
            newMove->colateralDamage = 1;

            SDL_zero(new_event);
            new_event.type = Event_Show; 
            new_event.user.data1 = newMove;

            SDL_PushEvent(&new_event);
            break;
        }
        //Inactivity (socket havent received msgs for 30secs, and returns -1)
        else{
            //Select pacman type
            if(ClientInfo[num].superPower == 1){
                newMove->type = 'S';
            }
            else{
                newMove->type = 'P';
            }

            newMove->num = num;
            newMove->colateralDamage = 1;
            newMove->colateralNum = num;
            newMove->colateralType = 'M';

            //Reading client actual positions
            pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
            newMove->x_old = ClientInfo[num].x_pac;
            newMove->y_old = ClientInfo[num].y_pac;
            newMove->colateral_x_old = ClientInfo[num].x_moob;
            newMove->colateral_y_old = ClientInfo[num].y_moob;
            pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

            //Generate random position for jump
            generateValidRandomPos(&(newMove->x_new), &(newMove->y_new));
            generateValidRandomPos(&(newMove->x_colateral), &(newMove->y_colateral));

            //Updating board places
            pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
            Board[newMove->x_new][newMove->y_new].occupied = 1;
            Board[newMove->x_new][newMove->y_new].ClientNum = num;
            Board[newMove->x_new][newMove->y_new].type = newMove->type;

            Board[newMove->x_old][newMove->y_old].occupied = 0;
            Board[newMove->x_old][newMove->y_old].type = ' ';
            Board[newMove->x_old][newMove->y_old].ClientNum = -1;

            Board[newMove->x_colateral][newMove->y_colateral].occupied = 1;
            Board[newMove->x_colateral][newMove->y_colateral].ClientNum = num;
            Board[newMove->x_colateral][newMove->y_colateral].type = newMove->colateralType;

            Board[newMove->colateral_x_old][newMove->colateral_y_old].occupied = 0;
            Board[newMove->colateral_x_old][newMove->colateral_y_old].type = ' ';
            Board[newMove->colateral_x_old][newMove->colateral_y_old].ClientNum = -1;
            pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

            //Updating Client positions
            pthread_rwlock_wrlock(&(SyncVars.rwLockClientInfo));
            ClientInfo[num].x_pac = newMove->x_new;
            ClientInfo[num].y_pac = newMove->y_new;
            ClientInfo[num].x_moob = newMove->x_colateral;
            ClientInfo[num].y_moob = newMove->y_colateral;
            pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

            newMove->colateralPaintColour[0] = ClientInfo[num].colour[0];
            newMove->colateralPaintColour[1] = ClientInfo[num].colour[1];
            newMove->colateralPaintColour[2] = ClientInfo[num].colour[2];
            newMove->paintColour[0] = ClientInfo[num].colour[0];
            newMove->paintColour[1] = ClientInfo[num].colour[1];
            newMove->paintColour[2] = ClientInfo[num].colour[2];

            SDL_zero(new_event);
            new_event.type = Event_Show; 
            new_event.user.data1 = newMove;

            SDL_PushEvent(&new_event);
            
        }
	}

    //If client quit, inform fruitsThread to update number of fruits
    pthread_mutex_lock(&(SyncVars.muxFruits));
    numActiveClients--;
    pthread_cond_signal(&(SyncVars.condFruits));
    pthread_mutex_unlock(&(SyncVars.muxFruits));

    close(ClientInfo[num].clientfd);
    pthread_exit(0);
}


//Thread that waits and handles new clients connections
void *threadAccept(void *arg){
    
    int counter;
    socklen_t size_addr = sizeof(struct sockaddr_in);

    int gameFull;

    //message msg;
    while(1){
        //Chooses an available (unoccupied) position to store new client info
        pthread_rwlock_rdlock(&(SyncVars.rwLockActiveClient));
        gameFull = 1;
        for(int i = 0; i < MAXCLIENTS; i++){
            if(ClientInfo[i].clientActive == 0){
                counter = i;
                gameFull = 0;
                break;
            }
        }
        pthread_rwlock_unlock(&(SyncVars.rwLockActiveClient));
        
        if(gameFull == 0){
            printf("Waiting for new client (number %d) to connect\n", counter);
            ClientInfo[counter].clientfd = accept(sock_fd, (struct sockaddr*) &(ClientInfo[counter]).client_addr, &size_addr);
            if(ClientInfo[counter].clientfd == -1) {
                perror("accept");
                exit(-1);
            }
            /*int clientDgramConnected;
            socklen_t size_other_addr = sizeof(struct sockaddr_storage);
            int bytes = recvfrom(dgram_fd, &clientDgramConnected, sizeof(int), 0, 
                                    (struct sockaddr *) &(ClientInfo[counter].dgramClient_addr),
                                    &size_other_addr);
            if(bytes <= 0){
                perror("Couldnt receive 1st dgram msg from client:");
            }
            else{
                printf("Client connected %d\n", clientDgramConnected);
            }*/

            printf("Accepted connection from %s, Port %d\n", inet_ntoa(ClientInfo[counter].client_addr.sin_addr), ClientInfo[counter].client_addr.sin_port);

            //Creating thread to handle new client
            int arg = counter;
            pthread_create(&ClientInfo[counter].threadID, NULL, ClientThreadShow, &arg);

            //Writing to ClientInfo structure
            pthread_rwlock_wrlock(&(SyncVars.rwLockActiveClient));
            ClientInfo[counter].clientActive = 1;
            pthread_rwlock_unlock(&(SyncVars.rwLockActiveClient));

            Score[counter].activeClient = 1;

            //Signaling to update number of fruits
            pthread_mutex_lock(&(SyncVars.muxFruits));
            numActiveClients++;
            //printf("\t\tSignaling condFruits\n");
            pthread_cond_signal(&(SyncVars.condFruits));
            pthread_mutex_unlock(&(SyncVars.muxFruits));

            
        }
        //Server can´t place more players in board
        else{
            printf("Game Board is Full. Next Client will be kicked out. \n");
            struct sockaddr_in clientKick_addr;
            int clientfd = accept(sock_fd, (struct sockaddr*) &clientKick_addr, &size_addr);
            if(ClientInfo[counter].clientfd == -1) {
                perror("accept when server is full");
                exit(-1);
            }
            printf("Going to kicked addr %s, Port %d\n", inet_ntoa(clientKick_addr.sin_addr), clientKick_addr.sin_port);

            close(clientfd);
            sleep(5);
        }
    }
}