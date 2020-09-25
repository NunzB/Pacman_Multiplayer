#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <SDL2/SDL.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#include "ServLib.h"

//int MAXCLIENTS = 5;
int numActiveClients = 0;

char ApplyGameRules(message *msg, ServMsg *newMove, int pos_old[]){

    int num = (*msg).num;
    int x_new = (*msg).x;
    int y_new = (*msg).y;
    char type = (*msg).type;

    int *CharPos_x, *CharPos_y;

    //Consoante type, trata-se da posição do pacman ou moob
    if(type == 'P'){
        CharPos_x = &(ClientInfo[num].x_pac);
        CharPos_y = &(ClientInfo[num].y_pac);
        if(ClientInfo[num].superPower == 1){
            type = 'S';
        }
    }
    else if(type == 'M'){
        CharPos_x = &(ClientInfo[num].x_moob);
        CharPos_y = &(ClientInfo[num].y_moob);
    }
    
    int x_old, y_old, colateral_x_old, colateral_y_old;

    pthread_mutex_lock(&(SyncVars.muxGameRules));

    //Retrieve old position
    pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
    x_old = *CharPos_x;
    y_old = *CharPos_y;
    pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

    if((x_old != pos_old[0]) || (y_old != pos_old[1])){     //Position has changed since play was analysed (due to colateral damage)
        pthread_mutex_unlock(&(SyncVars.muxGameRules));
        return 'W';
    }

    //printf("NewPlace [%d][%d], OldPlace [%d][%d] ", x_new, y_new, x_old, y_old);

    int newPlaceNum;
    int newPlaceOccupied;
    char newPlaceType;
    int outOfBoard;

    //To check if new move is not outside board
    if(((x_new >= 0) && (x_new < BoardSize[0])) && ((y_new >= 0) && (y_new < BoardSize[1]))){
        pthread_rwlock_rdlock(&(SyncVars.rwLockBoard));
        newPlaceNum = Board[x_new][y_new].ClientNum;
        newPlaceOccupied = Board[x_new][y_new].occupied;
        newPlaceType = Board[x_new][y_new].type;
        pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

        outOfBoard = 0;
    }
    else{ //Move outside board
        newPlaceNum = 0;
        newPlaceOccupied = 0;
        newPlaceType = ' ';
        outOfBoard = 1;
    }

    //printf("OutOfBoard %d\n", outOfBoard);
    int actionMade;
    char interaction = 'N';

    //(Movement in y) AND (out of board OR into brick) -> Change y_new
    if( (x_new == x_old) && ((outOfBoard == 1) || ((newPlaceOccupied == 1) && (newPlaceType == 'B')) ) ){   
        if((y_new < 0) || (y_new < y_old)){
            y_new += 2;
        }
        else if((y_new >= BoardSize[1]) || (y_new > y_old)){
            y_new -= 2;
        }

        actionMade = 1;
        interaction = 'B';

    }
    
    //(Movement in x) AND (out of board OR into brick) -> Change x_new
    else if( (y_new == y_old) && ( (outOfBoard == 1) || ((newPlaceOccupied == 1) && (newPlaceType == 'B')) ) ){  
        if((x_new < 0) || (x_new < x_old)){
            x_new += 2;
        }
        else if((x_new >= BoardSize[0]) || (x_new > x_old)){
            x_new -= 2;
        }
        
        actionMade = 1;
        interaction = 'B';
    }
    else{   //Movement inside board, everything's fine. Let's check new place
        actionMade = 0;
    }

    //If bounced to a place inside board
    if((actionMade == 1) && (((x_new >= 0) && (x_new < BoardSize[0])) && ((y_new >= 0) && (y_new < BoardSize[1])))){ 
        pthread_rwlock_rdlock(&(SyncVars.rwLockBoard));
        newPlaceNum = Board[x_new][y_new].ClientNum;
        newPlaceOccupied = Board[x_new][y_new].occupied;
        newPlaceType = Board[x_new][y_new].type;
        pthread_rwlock_unlock(&(SyncVars.rwLockBoard));
    }
    else if(actionMade == 1){    //if Bounced to out of board
        x_new = x_old;
        y_new = y_old;
        newPlaceOccupied = 0;
        interaction = 'B';
    }

    int x_colateral, y_colateral, colateralDamage;
    colateralDamage = 0;
    
    //If move to an occupied Place
    if(newPlaceOccupied == 1){   
        
        //printf("Place [%d][%d] Occ:%d, type:%c, num:%d\n", x_new, y_new, newPlaceOccupied, newPlaceType, newPlaceNum);

        //Brick obstacle -> Only happens when char is stuck between 2 obstacles -> Keep old position
        if(newPlaceType == 'B'){      
            x_new = x_old;
            y_new = y_old;
            interaction = 'B';
        }

        //Pacman obstacle on the desired position
        else if((newPlaceType == 'P') || (newPlaceType == 'S')){  
            //If the char to move it's a pacman or superPac -> switches position  
            if((type == 'P') || (type == 'S') ){     
                //Char C indicates position switch
                interaction = 'C';
                
                colateralDamage = 1;
                colateral_x_old = x_new;
                colateral_y_old = y_new;
                x_colateral = x_old;
                y_colateral = y_old;

            }
            //If the char to move it's a monster 
            else if(type == 'M'){    
                if(newPlaceNum == num){   //if M and P from same player
                    interaction = 'C';

                    colateralDamage = 1;
                    colateral_x_old = x_new;
                    colateral_y_old = y_new;
                    x_colateral = x_old;
                    y_colateral = y_old;
                }
                else{                   // if M and P from diff players
                    //is superPac on new pos -> monster is eaten
                    if(newPlaceType == 'S'){
                        interaction = 'E';

                        Score[newPlaceNum].superPacEaten++;

                        ClientInfo[newPlaceNum].superPowerEats++;
                        if(ClientInfo[newPlaceNum].superPowerEats == 2){
                            ClientInfo[newPlaceNum].superPower = 0;
                            colateralDamage = 1;
                            colateral_x_old = x_new;
                            colateral_y_old = y_new;
                            x_colateral = x_new;
                            y_colateral = y_new;
                            newPlaceType = 'P';
                        }
                        generateValidRandomPos(&x_new, &y_new);
                    }
                    //if Pac on new pos -> monster eats
                    else{
                        interaction = 'E';

                        colateralDamage = 1;
                        generateValidRandomPos(&x_colateral, &y_colateral);
                        colateral_x_old = x_new;
                        colateral_y_old = y_new;

                        Score[num].monsterEaten++;
                    }
                }
            }
                
        }
        //Monster obstacle on the desired position
        else if(newPlaceType == 'M'){   
            //If the char to move it's a pac -> Eaten or Change
            if((type == 'P') || (type == 'S')){                
                if(newPlaceNum == num){   //if M and P from same player
                    interaction = 'C';

                    colateralDamage = 1;
                    colateral_x_old = x_new;
                    colateral_y_old = y_new;
                    x_colateral = x_old;
                    y_colateral = y_old;
                }
                else{               //if M and P from diff players
                    //if superPowerPac -> eats monster
                    if(type == 'S'){
                        interaction = 'E';
                        colateralDamage = 1;
                        generateValidRandomPos(&x_colateral, &y_colateral);
                        colateral_x_old = x_new;
                        colateral_y_old = y_new;
                        ClientInfo[num].superPowerEats++;
                        if(ClientInfo[num].superPowerEats == 2){
                            ClientInfo[num].superPower = 0;
                            type = 'P';
                        }

                        Score[num].superPacEaten++;
                    }
                    //if normal pac -> is eaten by monster
                    else{
                        interaction = 'E';
                        generateValidRandomPos(&x_new, &y_new);

                        Score[newPlaceNum].monsterEaten++;
                    }
                    
                }
            }
            //If the char to move its a monster -> switches pos
            else if(type == 'M'){       
                interaction = 'C';

                colateralDamage = 1;
                colateral_x_old = x_new;
                colateral_y_old = y_new;
                x_colateral = x_old;
                y_colateral = y_old;
                
            }
            
        }
        //Fruit on the desired position
        else if(newPlaceType == 'F'){   
            //If the char to move it's a pac -> turn superPower Pac
            if((type == 'P') || (type == 'S')){
                interaction = 'S';
                type = 'S';
                ClientInfo[num].superPower = 1;
                ClientInfo[num].superPowerEats = 0;
                
                pthread_mutex_lock(&(SyncVars.muxFruits));
                pthread_cond_signal(&(SyncVars.condFruits));
                pthread_mutex_unlock(&(SyncVars.muxFruits));

                Score[num].fruitEaten++;
            }
            else{
                interaction = 'E';
                pthread_mutex_lock(&(SyncVars.muxFruits));
                pthread_cond_signal(&(SyncVars.condFruits));
                pthread_mutex_unlock(&(SyncVars.muxFruits));

            }
        }
    }
    

    //Update client position in clientInfo structure
    pthread_rwlock_wrlock(&(SyncVars.rwLockClientInfo));
    *CharPos_x = x_new;
    *CharPos_y = y_new;
    pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

    //New place is occupied by this client
    pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
    Board[x_new][y_new].ClientNum = num;
    Board[x_new][y_new].occupied = 1;
    Board[x_new][y_new].type = type;
    
    //Old place is set unoccupied;
    Board[x_old][y_old].occupied = 0;
    Board[x_old][y_old].type = ' ';
    Board[x_old][y_old].ClientNum = -1;
    pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

    if(colateralDamage == 1){
        //Update colateral client info
        pthread_rwlock_wrlock(&(SyncVars.rwLockClientInfo));
        if((newPlaceType == 'P') || (newPlaceType == 'S')){
            ClientInfo[newPlaceNum].x_pac = x_colateral;
            ClientInfo[newPlaceNum].y_pac = y_colateral;
        }
        else if(newPlaceType == 'M'){
            ClientInfo[newPlaceNum].x_moob = x_colateral;
            ClientInfo[newPlaceNum].y_moob = y_colateral;
        }
        pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));
        
        //Old place is occupied by the colateral client
        pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
        Board[x_colateral][y_colateral].ClientNum = newPlaceNum;
        Board[x_colateral][y_colateral].occupied = 1;
        Board[x_colateral][y_colateral].type = newPlaceType;
        pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

    }

    pthread_mutex_unlock(&(SyncVars.muxGameRules));

    memset(newMove, 0, sizeof(ServMsg));

    newMove->colateralDamage = colateralDamage;
    newMove->colateralType = newPlaceType;
    newMove->x_new = x_new;
    newMove->y_new = y_new;
    newMove->x_old = x_old;
    newMove->y_old = y_old;
    newMove->type = type;
    newMove->x_colateral = x_colateral;
    newMove->y_colateral = y_colateral;
    newMove->colateral_x_old = colateral_x_old;
    newMove->colateral_y_old = colateral_y_old;
    newMove->num = num;
    newMove->colateralNum = newPlaceNum;
    newMove->colateralPaintColour[0] = ClientInfo[newPlaceNum].colour[0];
    newMove->colateralPaintColour[1] = ClientInfo[newPlaceNum].colour[1];
    newMove->colateralPaintColour[2] = ClientInfo[newPlaceNum].colour[2];
    newMove->paintColour[0] = ClientInfo[num].colour[0];
    newMove->paintColour[1] = ClientInfo[num].colour[1];
    newMove->paintColour[2] = ClientInfo[num].colour[2];

    return interaction;
    
}

void setupSharedMemory(){
    key = ftok("/tmp", 'S');
    int size = (sizeof(sharedScore *) * MAXCLIENTS);
    if ((shmid = shmget(key, size, IPC_CREAT | 0666)) == -1) {
        perror("shmget");
        exit(1);
    }

    Score = (sharedScore *) shmat(shmid, NULL, 0);
    if (Score == (sharedScore *)(-1)) {
        perror("shmat");
        exit(1);
    }
    memset(Score, 0, (MAXCLIENTS*sizeof(sharedScore)));

    ScoreBoardInit = get_time_usecs();
    return;
}

void paintFruitEvent(){
    ServMsg *newMove = (ServMsg *) malloc(sizeof(ServMsg));
    memset(newMove, 0, sizeof(ServMsg));
    int x, y;
    srand(time(NULL));
    generateValidRandomPos(&x, &y);

    newMove->type = 'F';
    newMove->colateralDamage = rand()%2;
    if(newMove->colateralDamage == 1){
        newMove->x_colateral = x;
        newMove->y_colateral = y;
    }
    else{
        newMove->x_new = x;
        newMove->y_new = y;
    }
    SDL_Event new_event;
    SDL_zero(new_event);
    new_event.type = Event_Show;
    new_event.user.data1 = newMove;

    pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
    Board[x][y].occupied = 1;
    Board[x][y].ClientNum = -1;
    Board[x][y].type = 'F';
    pthread_rwlock_unlock(&(SyncVars.rwLockBoard));
    
    SDL_PushEvent(&new_event);  


    return;
}

void initializeSyncVars(){
    int ret_code;

    ret_code = pthread_mutex_init(&(SyncVars.muxGameRules), NULL);
    if(ret_code != 0){
        perror("Game Rules Mutex init: ");
    }

    ret_code = pthread_mutex_init(&(SyncVars.muxFruits), NULL);
    if(ret_code != 0){
        perror("Fruit Mutex init: ");
    }
    
    ret_code = pthread_rwlock_init(&(SyncVars.rwLockBoard), NULL);
    if(ret_code != 0){
        perror("Board RW Lock init: ");
    }

    ret_code = pthread_rwlock_init(&(SyncVars.rwLockClientInfo), NULL);
    if(ret_code != 0){
        perror("ClientInfo RW Lock init: ");
    }

    ret_code = pthread_rwlock_init(&(SyncVars.rwLockActiveClient), NULL);
    if(ret_code != 0){
        perror("ClientInfo RW Lock init: ");
    }

    ret_code = pthread_cond_init(&(SyncVars.condFruits), NULL);
    if(ret_code != 0){
        perror("Fruit Cond Var init: ");
    }

    ret_code = pthread_cond_init(&(SyncVars.condTimeFruits), NULL);
    if(ret_code != 0){
        perror("Time Fruit Cond Var init: ");
    }

    return;
}


//Setup socket to receive client connections
void servSocketSetup(){
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(3000);

    //Creating Stream Socket - Used to update clients characters position
    sock_fd= socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1){
		perror("Creating socket: ");
		exit(-1);
	}
    
    //Bind Sockets
    int on = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    int err = bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(err == -1) {
        perror("bind Stream socket:");
        ctrl_c_func();
    }
    
    if(listen(sock_fd, 5) == -1) {
        perror("listen");
        exit(-1);
    }

   /* //Dgram Socket to send score board every minute
    dgram_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (dgram_fd == -1){
        perror("Dgram socket: ");
        exit(-1);
    }

    struct sockaddr_in localDgram_addr;
    localDgram_addr.sin_family = AF_INET;
    localDgram_addr.sin_addr.s_addr = INADDR_ANY;
    localDgram_addr.sin_port = htons(3009);

    err = bind(dgram_fd, (struct sockaddr *) &localDgram_addr, sizeof(localDgram_addr));
	if(err == -1) {
		perror("Dgram bind");
		exit(-1);
	}
*/
    return;
}

//Manage ctrl-c
void ctrl_c_func(){
    int done;
    pthread_mutex_destroy(&(SyncVars.muxGameRules));
    pthread_mutex_destroy(&(SyncVars.muxFruits));
    pthread_rwlock_destroy(&(SyncVars.rwLockActiveClient));
    pthread_rwlock_destroy(&(SyncVars.rwLockBoard));
    pthread_rwlock_destroy(&(SyncVars.rwLockClientInfo));
    pthread_cond_destroy(&(SyncVars.condFruits));

    if (shmdt(Score) == -1) {
            perror("shmdt");
            exit(1);
    }
    if(-1 == (shmctl(shmid, IPC_STAT, NULL)))
    {   
        perror("shmctl");
    }   

    if(-1 == (shmctl(shmid, IPC_RMID, NULL)))
    {   
        perror("shmctl");
    } 


	close(sock_fd);
    free(ClientInfo);
    //unlink(SOCK_ADDRESS);
    printf("Closing and unlinking sockets\n");
    exit(0);
}


//Sending position of new client to other clients
void UpdateOldClientsScreen(int counter, ServMsg * Smsg){   
    
    Smsg->type = 'N';
    Smsg->num = -1;
    Smsg->colateralDamage = 1;
    Smsg->colateralType = ' ';
    Smsg->paintColour[0] = ClientInfo[counter].colour[0];
    Smsg->paintColour[1] = ClientInfo[counter].colour[1];
    Smsg->paintColour[2] = ClientInfo[counter].colour[2];

    pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
    Smsg->x_new = ClientInfo[counter].x_pac;
    Smsg->y_new = ClientInfo[counter].y_pac;
    Smsg->x_colateral = ClientInfo[counter].x_moob;
    Smsg->y_colateral = ClientInfo[counter].y_moob;
    pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));
    

    //send(ClientInfo[i].clientfd, &Smsg, sizeof(Smsg), 0);
    
    return;
}


//Sends position of other objects in board to the new client
void UpdateNewClientScreen(int boardSize_x, int boardSize_y, int counter){  
    //printf("Board 0 0: Occupied %d; Type %c; clientNum %d\n", Board[0][0].occupied, Board[0][0].type, Board[0][0].ClientNum);
    
    //Initialize lastCharacter play time instance
    ClientInfo[counter].lastPacPlay = 0;
    ClientInfo[counter].lastMoobPlay = 0;
    
    //Sends every brick position to client
    for(int i = 0; i < boardSize_x; i++){
        for(int j = 0; j < boardSize_y; j++){
            if((Board[i][j].occupied == 1) && (Board[i][j].type == 'B')){
                ServMsg Smsg;
                Smsg.type = 'N';
                Smsg.colateralDamage = 1;
                Smsg.colateralType = 'B';
                Smsg.x_new = i;
                Smsg.y_new = j;
                //printf("Sending: [%d][%d] type %c, subtype: %c\n", Smsg.x_new, Smsg.y_new, Smsg.type, Smsg.colateralType);
                send(ClientInfo[counter].clientfd, &Smsg, sizeof(Smsg), 0);
            }
            else if((Board[i][j].occupied == 1) && (Board[i][j].type == 'F')){
                ServMsg Smsg;
                Smsg.type = 'F';
                srand(time(NULL));
                Smsg.colateralDamage = rand()%2;
                //Smsg.colateralType = 'F';
                if(Smsg.colateralDamage == 1){
                    Smsg.x_colateral = i;
                    Smsg.y_colateral = j;
                }
                else{
                    Smsg.x_new = i;
                    Smsg.y_new = j;
                }
                
                //printf("Sending: [%d][%d] type %c, subtype: %c\n", Smsg.x_new, Smsg.y_new, Smsg.type, Smsg.colateralType);
                send(ClientInfo[counter].clientfd, &Smsg, sizeof(Smsg), 0);
            }
        }
    }
    int active;
    for(int i = 0; i < MAXCLIENTS; i++){

        pthread_rwlock_rdlock(&(SyncVars.rwLockActiveClient));
        active = ClientInfo[i].clientActive;
        pthread_rwlock_unlock(&(SyncVars.rwLockActiveClient));

        if(( active == 1) && (i != counter)){
            ServMsg Smsg;
            Smsg.type = 'N';
            Smsg.num = -1;
            Smsg.colateralDamage = 1;
            if(ClientInfo[i].superPower == 1){
                Smsg.colateralType = 'S';
            }
            else{
                Smsg.colateralType = 'P';
            }
            
            Smsg.paintColour[0] = ClientInfo[i].colour[0];
            Smsg.paintColour[1] = ClientInfo[i].colour[1];
            Smsg.paintColour[2] = ClientInfo[i].colour[2];
            
            pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
            Smsg.x_new = ClientInfo[i].x_pac;
            Smsg.y_new = ClientInfo[i].y_pac;
            Smsg.x_colateral = ClientInfo[i].x_moob;
            Smsg.y_colateral = ClientInfo[i].y_moob;
            pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));
            
            //printf("New Client Screen->Sending: i:%d, [%d][%d] type %c, colateral[%d][%d]: %c, colour: %d %d %d\n", i, Smsg.x_new, Smsg.y_new, Smsg.type, Smsg.x_colateral, Smsg.y_colateral, Smsg.colateralType, Smsg.paintColour[0], Smsg.paintColour[1], Smsg.paintColour[2]);
            send(ClientInfo[counter].clientfd, &Smsg, sizeof(Smsg), 0);
        }
    }
    return;
}      


//Reads brick location from file, allocates board structure and fill it
int GenerateBoardStructure(int size_x, int size_y){    //size_x -> colunas, size_y ->linhas
    BoardSize[0] = size_x;
    BoardSize[1] = size_y;
    Board = malloc(sizeof(struct BoardInfo * )*BoardSize[0]);
    for(int i = 0; i < size_x; i++){
        Board[i] = malloc(sizeof(struct BoardInfo)*BoardSize[1]);
    }
    
    //Call board-gen to create board brick positions
    pid_t pid=fork();
    if (pid==0) {  //child process 
        char command[20] = "./board-gen ";
        char argv1[] = "00";
        char argv2[] = "00";
        
        sprintf(argv1, "%d", size_x);
        sprintf(argv2, "%d", size_y);

        strcat(command, argv1);
        strcat(command, " ");
        strcat(command, argv2);

        system(command);
       
        exit(0);  
    }
    else if (pid!=0){ //parent process 
        waitpid(pid,0,0); // wait for child to exit 
    }
    
    FILE * fp;
    if((fp = fopen("board.txt", "r")) == NULL){
        perror("Open board.txt file: ");
        exit(-1);
    }

    char *Temp = malloc((size_x) * sizeof(char) );
    fgets(Temp, 100, fp);
    int numBricks = 0;
    for(int i = 0 ; i < size_y; i++){
        fgets(Temp, 100, fp); //It stops when it reads a \n
        printf("%s\n", Temp);
        for(int j = 0; j < size_x; j++){
            Board[j][i].type = Temp[j];
            if(Board[j][i].type == 'B'){
                Board[j][i].occupied = 1;
                Board[j][i].ClientNum = -1;
                numBricks++;
            }
            else{
                Board[j][i].occupied = 0;
                Board[j][i].ClientNum = 0;
            }
            
            //printf("Board[%d][%d] Occupied %d\n", j, i, Board[j][i].occupied);
        }
	}
    free(Temp);

    //printf("Board Generated\n");

    return numBricks;
}


//Receive and store new client colour
void RetrieveNewClientColour(int counter){
    int newClientColor[3];
    if(recv(ClientInfo[counter].clientfd, newClientColor, sizeof(newClientColor), 0) <= 0){
        perror("read newClient colour: ");
        ctrl_c_func(); 
    }
    ClientInfo[counter].colour[0] = newClientColor[0];
    ClientInfo[counter].colour[1] = newClientColor[1];
    ClientInfo[counter].colour[2] = newClientColor[2];
    //printf("New colour: %d %d %d\n", ClientInfo[counter].colour[0], ClientInfo[counter].colour[1], ClientInfo[counter].colour[2]);

    return;
}


//Gets time now in usec
long int get_time_usecs(){	
    double t1;
    struct timeval ts;

    gettimeofday(&ts, NULL);  

    t1 = ts.tv_sec*1.0e6 + ts.tv_usec;  

    return t1;
}


//Analyse play: return 1 if it's valid and 0 otherwise.
//Valid play -> Up, down, left or right, and deltaT > 0.5secs
int AnalyseClientPlay(message msg, int pos[]){
    int x_new = msg.x;
    int y_new = msg.y;
    int num = msg.num;
    int validPlay = 0;
    int possiblePlayX[2];
    int possiblePlayY[2];
    int x_old, y_old;

    useconds_t minTimeBetweenPlays = 500000;
    double t1 = get_time_usecs();

    int *CharPos_x, *CharPos_y;

    if(msg.type == 'P'){
        if((t1 - ClientInfo[num].lastPacPlay) < minTimeBetweenPlays){
            return validPlay;
        }
        CharPos_x = &(ClientInfo[num].x_pac);
        CharPos_y = &(ClientInfo[num].y_pac);
    }
    else if(msg.type == 'M'){
        if((t1 - ClientInfo[num].lastMoobPlay) < minTimeBetweenPlays){
            return validPlay;
        }
        CharPos_x = &(ClientInfo[num].x_moob);
        CharPos_y = &(ClientInfo[num].y_moob);
    }
    else{
        return 0;
    }

    pthread_rwlock_rdlock(&(SyncVars.rwLockClientInfo));
    x_old = *CharPos_x;
    y_old = *CharPos_y;
    pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

    possiblePlayX[0] = x_old - 1;
    possiblePlayX[1] = x_old + 1;
    possiblePlayY[0] = y_old - 1;
    possiblePlayY[1] = y_old + 1;

    for(int i = 0; i < 2; i++){
        if((y_new == possiblePlayY[i]) && (x_new == x_old)){   //Movement in x
                validPlay = 1;
        }
        if((x_new == possiblePlayX[i]) && (y_new == y_old)){   //Movement in y
                validPlay = 1;
        }
    }

    pos[0] = x_old;
    pos[1] = y_old;

    return validPlay;
}


void generateValidRandomPos(int* x, int* y){
    int done = 0;
    
    while(!done){
        //Critical Region
        *x = rand()%BoardSize[0];
        *y = rand()%BoardSize[1];

        pthread_rwlock_rdlock(&(SyncVars.rwLockBoard));
        if(Board[*x][*y].occupied == 0){
            done = 1;
            //printf("\tPlace [%d][%d] occupied %d\n", *x, *y, Board[*x][*y].occupied);
        }
        pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

        //printf("New client pac [%d][%d] - Occupied %d, type %c\n", ClientInfo[counter].x_pac, ClientInfo[counter].y_pac, 
        //        Board[ClientInfo[counter].x_pac][ClientInfo[counter].y_pac].occupied, Board[ClientInfo[counter].x_pac][ClientInfo[counter].y_pac].type);
    }
    return;
}

//Generates new client initial message and assigns a random non-occupied position for its characters
void GenerateInitialMessage(int counter, messageInit *msgInit){   
    int x_pac, y_pac, x_moob, y_moob;
    
    msgInit->clientNum = counter;
    msgInit->boardSize_x = BoardSize[0];
    msgInit->boardSize_y = BoardSize[1];
    msgInit->MAXCLIENTS = MAXCLIENTS;

    //Generate random initial position
    int done = 0;
    srand(time(NULL));

    int deltaT = (get_time_usecs() - ScoreBoardInit)%60000000;
    
    
    deltaT = deltaT / (1000000);

    deltaT = 60 - deltaT;

    msgInit->timeToShowScore = deltaT;


    //Critical Region
    generateValidRandomPos(&x_pac, &y_pac);
    generateValidRandomPos(&x_moob, &y_moob);

    pthread_rwlock_wrlock(&(SyncVars.rwLockBoard));
    Board[x_pac][y_pac].occupied = 1;
    Board[x_pac][y_pac].ClientNum = counter;
    Board[x_pac][y_pac].type = 'P';

    Board[x_moob][y_moob].occupied = 1;
    Board[x_moob][y_moob].ClientNum = counter;
    Board[x_moob][y_moob].type = 'M';
    pthread_rwlock_unlock(&(SyncVars.rwLockBoard));

    pthread_rwlock_wrlock(&(SyncVars.rwLockClientInfo));
    ClientInfo[counter].x_pac = x_pac;
    ClientInfo[counter].y_pac = y_pac;
    ClientInfo[counter].x_moob = x_moob;
    ClientInfo[counter].y_moob = y_moob;
    pthread_rwlock_unlock(&(SyncVars.rwLockClientInfo));

    msgInit->pac[0] = x_pac;
    msgInit->pac[1] = y_pac;
    msgInit->moob[0] = x_moob;
    msgInit->moob[1] = y_moob;

    //printf("New client pac [%d][%d] - Occupied %d, type %c\n", ClientInfo[counter].x_pac, ClientInfo[counter].y_pac, Board[ClientInfo[counter].x_pac][ClientInfo[counter].y_pac].occupied, Board[ClientInfo[counter].x_pac][ClientInfo[counter].y_pac].type);

    //printf("New client moob [%d][%d] - Occupied %d\n", ClientInfo[counter].x_moob, ClientInfo[counter].y_moob, Board[ClientInfo[counter].x_moob][ClientInfo[counter].y_moob].occupied);

    return;
}