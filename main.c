//CS 344
//Chris Asbury
//Assignment 3 smallSH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>

void init_shell(); //clear screen and print user
void printStatus(int *status, char *fileOutput, int *fileOutputStat); //Handles printing the status of the last cmd
void cdHandler(char** parsed); //Handles cmd call to cd
char* redirectOut(char **args, int numArgs); //handles redirection output for >
char* redirectIn(char **args, int numArgs); //handles redirection input for <
void runShell(); //actually runs the shell contains the while loop to keep the process going
pid_t *checkbgprocesses(int *status, int bgCount, pid_t *bgProcesses); //checks current background processes and prints exit values
void saHandler(); //Main function for Handling SIGTSTP for ctrl z and ctrl c




int fgOnlyMode = 0;

#define MAXLET 2048 // max number of letters to be supported
#define MAXCMD 512 // max number of commands to be supported
#define clear() printf("\033[H\033[J") //function to clear screen before running





int main(int argc, char *argv[])
{
    //set default handlers
    struct sigaction ignore = {0};

    struct sigaction INTaction = {0};
    struct sigaction STPaction = {0};
    INTaction.sa_handler = SIG_IGN;
    STPaction.sa_handler = saHandler;
    sigfillset(&INTaction.sa_mask);
    sigfillset(&STPaction.sa_mask);
    sigaction(SIGINT, &INTaction, NULL);
    sigaction(SIGTSTP, &STPaction, NULL);
    ignore.sa_handler = SIG_IGN;
    
    runShell();
    return 0;
}


void runShell(){

    
    int status = 0;
    init_shell();
    //define variables
    int bgCount = 0;
    pid_t *bgProcesses = malloc(100 * sizeof(pid_t));

    //begin loop until exit is called
    while(1){
        size_t size = MAXLET;
        char *input = (char *) calloc(size, sizeof(char));


        //check background proccesses
        //status = checkbgprocesses(status, bgCount, &bgProcesses);
        bgProcesses = checkbgprocesses(&status, bgCount, bgProcesses);
        
        //take input
        //print prompt

        printf(":"); fflush(stdout);
        int input_length = getline(&input, &size, stdin);
        
       //process input
            //split input by tokenizeing by space and newline
            char *args[MAXCMD];
            int tokenAlloc[MAXCMD] = {-1};
            int numArgs = 0;
            
            //get args
            char *token = strtok(input, " \n");
            //split up args 
            while(token != NULL){
                args[numArgs] = token;
                token = strtok(NULL, " \n");
                
                numArgs++;
                
            }
    
            args[numArgs] = NULL;

            
            //check for background 
            int background = 0;

            if(numArgs - 1 > 0 && strcmp(args[numArgs-1], "&") == 0){ // check last argument if it is & and its not in foreground only mode then set background to on
                args[numArgs-1] = NULL; // replace & with NULL
                if(fgOnlyMode == 0){
                    background = 1;
                }
            }

            //Handle PID replacement
            char pid[10]; sprintf(pid, "%d", getpid());

            //replace $$ with pid
            int j=0;
            while(args[j] != NULL){
                if(strcmp(args[j], "$$") == 0){ // if the argument is $$ then replace with pid
                    args[j] = pid;
                }else{ //check for $$ within string
                    int i;
                    int argLength = strlen(args[j]);
                    for(i=0; i < argLength; i++){
                        if (args[j][i] == '$' && i + 1 < argLength && args[j][i + 1] == '$') { //remove && add pid
                            char *tempAlloc = malloc(argLength + strlen(pid) + 1);
                            strcpy(tempAlloc, args[j]);
                            tempAlloc[i] = '\0';
                            strcat(tempAlloc, pid);
                            strcat(tempAlloc, args[j] + i + 2);
                            args[j] = tempAlloc;
                            tokenAlloc[j] = 1;
                        }
                    }
                }
                j++;
            }

            //set I/O redirection
            char *fileInput = redirectIn(args, numArgs); char *fileOutput = redirectOut(args, numArgs);
            int inputFileStat = -5; int fileOutputStat = -5;
            //Open both files if called
            if(fileInput != NULL){
                inputFileStat = open(fileInput, O_RDONLY);
            }
            if(fileOutput != NULL){
                fileOutputStat = open(fileOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }

            
           
            
            if (args[0] == NULL || args[0][0] == '#') {//Handle Comments and empty lines
                free(input);
                continue;
            }else if(strcmp(args[0], "cd") == 0){//Handle CD Call
                cdHandler(args);
            }else if(strcmp(args[0], "status") == 0 || strcmp(input, "status\n") == 0){//Handle Status Call
                printStatus(&status, fileOutput, &fileOutputStat);
            }else if(strcmp(args[0], "exit") == 0){//Handle exit Call

                free(input);
                int i;
                // Loop through all processes and kill
                for (i = 0; i < bgCount; i++) {
                    kill(bgProcesses[i], SIGTERM);
                }

                // Free memory for array
                free(bgProcesses);
                exit(0);

            }else{//handle external commands
                pid_t spawnPid = -5; int childStat; int childPid;

                spawnPid = fork();
                if(spawnPid == -1){ //failed fork
                    perror("fork failed");
                    exit(1);
                    break;
                }else if(spawnPid == 0){ // Inside Child
                    
                    
                    if (background == 1){// if in background then ignore ctrl c, set to defualt if in foreground
                        struct sigaction INTaction = {0};
                        INTaction.sa_handler = SIG_IGN;
                        sigfillset(&INTaction.sa_mask);
                        sigaction(SIGINT, &INTaction, NULL);
                    }else {
                        struct sigaction INTaction = {0};
                        INTaction.sa_handler = SIG_DFL;
                        sigfillset(&INTaction.sa_mask);
                        sigaction(SIGINT, &INTaction, NULL);
                    }

                    //ignores ctrl z
                    struct sigaction STPaction = {0};
                    STPaction.sa_handler = SIG_IGN;
                    sigfillset(&STPaction.sa_mask);
                    STPaction.sa_flags = 0;
                    sigaction(SIGTSTP, &STPaction, NULL);


                    if(inputFileStat != -5){// If input redirection
                        if (inputFileStat == -1){//cant open specified file
                            printf("cannot open %s for input\n", fileInput);
                            fflush(stdout);
                            exit(1);
                        }else{ // take from file
                            dup2(inputFileStat, 0);
                            close(inputFileStat);
                        }
                    }else if(inputFileStat == -5 && background == 1 && fgOnlyMode == 0){//If no input redirection and in background
                        inputFileStat = open("/dev/null", O_RDONLY); //Error pulling from dir
                        if (inputFileStat == -1){
                            printf("cannot open %s for input\n", fileInput);
                            fflush(stdout);
                            exit(1);
                        }else{ // get from div
                            dup2(inputFileStat, 0);
                            close(inputFileStat);
                        }
                    }

                    if(fileOutputStat != -5){ // output redirection
                        if (fileOutputStat == -1){ //cant open specified file
                            printf("cannot open %s for output\n", fileOutput);
                            fflush(stdout);
                            exit(1);
                        }else{ //change output to file
                            dup2(fileOutputStat, 1);
                            close(fileOutputStat);
                        }
                    }else if(fileOutputStat == -5 && background == 1 && fgOnlyMode == 0){ // output not specified and  in background
                        fileOutputStat = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        
                        if(fileOutputStat == -1) { // if file doesnt open
                            printf("cannot open %s for output\n", fileOutput);
                            fflush(stdout);
                            exit(1);
                        }else{ //redirect to console
                            dup2(fileOutputStat, 1);
                            close(fileOutputStat);
                        }
                    }

                    // run cmd
                    execvp(args[0], args);

                    // If failed command
                    printf("%s: no such file or directory\n", args[0]);
                    fflush(stdout);

                    // Kill child if failed

                    exit(1);
                    break;
                }else{//Handle Parent function
                        if (background == 1 && fgOnlyMode == 0) {//Child is in Background handler
                            // Restore defualts signals
                            struct sigaction STPaction = {0};
                            struct sigaction INTaction = {0};
                            STPaction.sa_handler = saHandler;
                            INTaction.sa_handler = SIG_IGN;
                            sigfillset(&STPaction.sa_mask);
                            sigfillset(&INTaction.sa_mask);
                            sigaction(SIGTSTP, &STPaction, NULL);
                            sigaction(SIGINT, &INTaction, NULL);

                            printf("background pid is %d\n", spawnPid);
                            fflush(stdout);

                            //store Background processess to check later
                            bgProcesses[bgCount] = spawnPid;
                            bgCount++;

                            
                            
                        }else{//Child is in Foreground handler
                            // Restore defualts signals
                            struct sigaction STPaction = {0};
                            struct sigaction INTaction = {0};
                            STPaction.sa_handler = saHandler;
                            INTaction.sa_handler = SIG_IGN;
                            sigfillset(&STPaction.sa_mask);
                            sigfillset(&INTaction.sa_mask);
                            sigaction(SIGTSTP, &STPaction, NULL);
                            sigaction(SIGINT, &INTaction, NULL);

                            childPid = waitpid(spawnPid, &childStat, 0);
                            status = WEXITSTATUS(childStat);

                            
                            if (WIFEXITED(childStat)) { //if Child terminated normally
                                status = WEXITSTATUS(childStat);
                            }
                            if (WIFSIGNALED(childStat)) { //if child termionated abnormally
                                printf("terminated by signal %d\n", WTERMSIG(childStat));
                                status = -1 * WTERMSIG(childStat);
                                fflush(stdout);
                            }

                        }

                }      
            }  
            int i = 0;
            for (i = 0; i < MAXCMD; i++) {
                if (tokenAlloc[i] == 1) {
                    free(args[i]);
                }
            }      
        
        free(input);
    }
    int i = 0;
    for (i; i < bgCount; i++) {
        kill(bgProcesses[i], SIGKILL);
    }
    free(bgProcesses);
}

//toggle foreground only mode and display corresponding message
void saHandler(){
    if (fgOnlyMode == 1) { //turn off foreground only mode
        printf("\nExiting foreground-only mode\n"); fflush(stdout); fgOnlyMode = 0;
    }else{ //turn on forground only mode
        printf("\nEntering foreground-only mode (& is now ignored)\n"); fflush(stdout); fgOnlyMode = 1;
    }
}


// status command builtin
void printStatus(int *status, char *fileOutput, int *fileOutputStat)
{
    if (*status >= 0) { 
        printf("exit value %d\n", *status);
        fflush(stdout);
    }else{//print termination value to console
        printf("terminated by signal %d\n", *status * -1);
        fflush(stdout);
    }
}

//function to handle cd
void cdHandler(char** parsed){
    if(parsed[1] == NULL){
        chdir(getenv("HOME"));
    } else{
        chdir(parsed[1]);
    }
    return;
}


pid_t *checkbgprocesses(int *status, int bgCount, pid_t bgProcesses[]){
    if(bgCount > 0){ //Check for any background processess
        int i;
        for(i=0; i < bgCount; i++){ //Loop through existing background processes
            //collect background process status
            int bgStatus;
            pid_t pidBG = waitpid(bgProcesses[i], &bgStatus, WNOHANG);
            if(pidBG == bgProcesses[i]){ //process has ended
                
                //Print That the process has ended
                printf("background pid %d is done: ", pidBG);
                fflush(stdout);
                //Print Exit values and set status
                if(WIFSIGNALED(bgStatus)){ //Exited abnormally
                    
                    printf("terminated by signal %d\n", WTERMSIG(bgStatus));
                    *status = -1 * WTERMSIG(bgStatus);
                    fflush(stdout);
                }else if(WIFEXITED(bgStatus)){ //exited normally
                    
                    printf("exit value %d\n", WEXITSTATUS(bgStatus));
                    *status = WEXITSTATUS(bgStatus);
                    fflush(stdout);
                }

                //Reset current process
                bgProcesses[i] = -5;
            }

        }
    }

    return bgProcesses;//return new status

}

void init_shell()
{
    clear();
    char* username = getenv("USER");
    printf("\n\n\nUSER is: @%s", username);
    printf("\n");
    fflush(stdout);
    sleep(1);
    clear();
}

char* redirectIn(char **args, int numArgs)
{
    char* fileInput = NULL;
    int i = 0;
    //Check for < (input redirection)
    while (args[i] != NULL){
        if( args[i+1] != NULL && i+1 <= numArgs  && strcmp(args[i], "<") == 0){
            fileInput = args[i+1];
            //remove < and file name and shift args
            int j;
            for(j=i;  j < numArgs; j++){
                if(j+2 <= numArgs && args[j+2] == NULL){
                    args[j] = NULL;
                }else{
                    args[j] = args[j+2];
                }
            }
        }
        i++;
    }
    return fileInput;
}


char* redirectOut(char **args, int numArgs)
{
    char *fileOutput = NULL;
    int i = 0;
    //Check for > (input redirection)
    while (args[i] != NULL){
        if(args[i+1] != NULL && i+1 <= numArgs && strcmp(args[i], ">") == 0){
            fileOutput = args[i+1];
            //remove > and file name and shift args
            int j;
            for(j=i;  j < numArgs; j++){
                if(j+2 <= numArgs && args[j+2] == NULL){
                    args[j] = NULL;
                }else{
                    args[j] = args[j+2];
                }
            }
        }
        i++;
    }
    return fileOutput;
}



