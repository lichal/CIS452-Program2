#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#define MAX 1024
#define READ 0
#define WRITE 1
#define PARENT 0
#define CHILD 1
#define MAXPROCESS 100

/* Token structure. */
struct token{
    int flag;
    int dest;
    char mes[MAX-8];
};

void* readline();
void* tokenPass();
void sigHandler (int sigNum);

/* Size of the network. */
int size;

/* Define an integer to specify your position in array. */
int process = 0;

/* An array holds pipe descriptor. */
int fd[MAXPROCESS][2];

/* Each process keep track of their pid and their child pid. */
pid_t pid[2];

int main(int argc, char* argv[]){
    
    /* Checks if user input a correct size, if not, initialize size to 3. */
    if(argv[1] == NULL || atoi(argv[1]) < 2){
        printf("Size incorrectly defined, initialize network size to 3...\n");
        size = 3;
    }else{
        size = atoi(argv[1]);
        printf("Network size %d, initializing...\n", size);
    }
    
    /* Malloc a pointer to array of pid, so the root can access all pid. */
    pid_t *pidArray = malloc(sizeof(pid_t)*size);
    
    /* Initialize all pipes. */
    int i = 0;
    while(i < size){
        printf("Initialize pipe %d\n", i);
        if (pipe (fd[i]) < 0) {
            perror ("Pipe plumbing problem");
            exit(1);
        }
        i++;
    }
    
    /*
     * Loop to spawn declared size of process.
     * One parent will only spawn one child, then the child become
     * parent and spawn another child.
     * So it will be Parent->Child->Grandchild->Great Grandchild->etc..
     */
    i = 0;
    while(i < size){
        /* Run into an error when forking */
        if ((pid[CHILD] = fork()) < 0) {
            perror ("fork failed");
            exit(1);
        }
        
        /* You are actually the parent, don't need to fork again, so break. */
        if(pid[CHILD]){
            break;
        }
        
        /* Yes, you are the child. */
        if(!pid[CHILD]){
            /* Increment by 1 on each child spawn. */
            i++;
            process= i;
            /* Break, have enough process running */
            if(i == size-1){
                break;
            }
        }
    }

    pid[PARENT] = getpid();
    pidArray[process] = getpid();
    printf("Pid %d Position %d\n", pidArray[process], process);
    if(process == (size-1)){
        pid[CHILD] = pidArray[0];
    }
    
    /* The root process. */
    if(!process){
        /* Initialize a token structure. */
        struct token myToken;
        myToken.flag = 0;
        
        /* Close all pipes READ and WRITE not for current process. */
        int closePos = 0;
        while(closePos < size && closePos != process && closePos != size-1){
            close(fd[closePos][READ]);
            close(fd[closePos][WRITE]);
            closePos++;
        }
        /*Sleep for two second to wait for all process to initialize. */
        sleep(2);
        
        close(fd[process][READ]);
        close(fd[size-1][WRITE]);
        
        char buffer[1024];
        printf("Enter a message: ");
        if(fgets(buffer,sizeof(buffer),stdin) == NULL){
            perror("Failed to get user input");
            exit(1);
        }
        
        char dest[128];
        printf("Enter a destination: ");
        if(fgets(dest,sizeof(dest),stdin) == NULL){
            perror("Failed to get user input");
            exit(1);
        }
        int destination = atoi(dest);
        
        myToken.dest = destination;
        strcpy(myToken.mes, buffer);
        
        printf("\nInitial token at process %d PID is %d\n", process, getpid());
        printf("Passing..\n\n");
        sleep(2);
        myToken.flag+=1;
        write(fd[process][WRITE], (struct token*) &myToken, MAX);
        signal(SIGINT, sigHandler);
        struct token *tok = malloc(sizeof(struct token));
        while(!process){
            int strNum = read(fd[size-1][READ], (struct token*) tok, MAX);
            if (strNum > MAX) {
                perror ("pipe read error\n");
                exit(1);
            }
            
            /* Obtain the token, and destination flag is set to zero. */
            // Holding the token.
            if(tok->flag == process){
                printf("Token passed to process %d PID is %d\n", process, getpid());
                
                // Desination is the root, cleaning token message field.
                if(tok->dest == process || tok->dest == getpid()){
                    /* Remove message and set destination to -1.. */
                    printf("Cleaning token information field...\n\n");
                    tok->dest = -1;
                    strcpy(tok->mes, "");
                }else{
                    printf("Don't need it, passing token.\n\n");
                }
            }
            sleep(2);
            tok->flag+=1;
            write(fd[process][WRITE], (struct token*) tok, MAX);

        }
    }
    
    if(process){
        char str[1024];
        
        /* Close all pipes READ and WRITE not for current process. */
        int closePos = 0;
        while(closePos < size && closePos != process && closePos != process-1){
            close(fd[closePos][READ]);
            close(fd[closePos][WRITE]);
            closePos++;
        }
        /* Use dup2 to duplicate previous pipe read to STDIN_FILENO */
        dup2(fd[process-1][READ], STDIN_FILENO);
        /* Close one of the previous pipe write permission */
        close(fd[process-1][WRITE]);
        close(fd[process-1][READ]);
        /* Close all write permission right now for printing. */
        close(fd[process][READ]);
        
        /* Token structure to hold the token. */
        struct token *tok;
        
        while(1){
            // Receiving message from the pipe.
            int strNum = read(STDIN_FILENO, (struct token*) str, MAX);
            if (strNum > MAX) {
                perror ("pipe read error\n");
                exit(1);
            }
            
            tok = (struct token *) str;
            
            /* Holding the token */
            if(tok->flag == process){
                printf("Token passed to process %d PID is %d\n", process, getpid());
                /* I'm the destination. */
                if(tok->dest==process || tok->dest==getpid()){
                    tok->dest=0;
                    printf("A message for process %d: %s\n", process, tok->mes);
                }else{
                    printf("Don't need it, passing token.\n\n");
                }
            }
            /* Move the token to the next flag. */
            tok->flag+=1;
            /* Set flag to zero if this is the last process in the structure. */
            if(tok->flag==size){
                tok->flag=0;
            }
            sleep(2);
            // Sending the message to the pipe.
            write(fd[process][WRITE], (struct token*) tok, MAX);
        }
    }
}

void* readline(){
//    while(1){
////        printf("Enter a message: ");
//        if(fgets(buffer,sizeof(buffer),stdin) == NULL){
//            perror("Failed to get user input");
//            exit(1);
//        }
////        printf("Enter a destination: ");
//        char dest[4];
//        if(fgets(dest,sizeof(dest),stdin) == NULL){
//            perror("Failed to get user input");
//            exit(1);
//        }
//        destination = atoi(dest);
//        kill(getpid(), SIGUSR1);
//    }
}

void* tokenPass(){
//    char str[1024];
//    while(!process){
//        signal(SIGUSR1, sigHandler);
//        int strNum = read(fd[size-1][READ], (struct token*) myToken, MAX);
//        myToken = (struct token *) myToken;
////        printf("Position: %d FLAG: %d, Message: %s\n", process, tok->flag, tok->mes);
//        /* Obtain the token, and destination flag is set to zero. */
//        // Holding the token.
//        if(myToken->flag == process){
//            readyToGrab = 1;
//            printf("Token passed to process %d PID is %d\n", process, getpid());
//
//            if(myToken->dest != -1){
//                printf("MYTOKEN %s", myToken->mes);
//            }
//            // Desination is the root, cleaning token message field.
//            if(myToken->dest==process || myToken->dest==getpid()){
//                /* Remove message and set destination to -1.. */
//                printf("Cleaning token information field...");
//                myToken->dest=-1;
//                strcpy(myToken->mes, "");
//            }else{
//                printf("22MYTOKEN %s", myToken->mes);
//                printf("Don't need it, passing token.\n\n");
//            }
//        }
//        sleep(2);
//        myToken->flag+=1;
//        write(fd[process][WRITE], (struct token*) myToken, MAX);
//    }
//    return myToken;
}

// This function handles signal SIGINT and terminates the processes
void sigHandler (int sigNum)
{
    if(sigNum == SIGINT){  /* Root process. */
        printf (" received. That’s it, I'm shutting you down...\n");
        // this is where shutdown code would be inserted
        sleep (2);
        exit(0);
    }
}





