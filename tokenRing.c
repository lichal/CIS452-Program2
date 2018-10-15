#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>


#define MAX 1024
#define READ 0
#define WRITE 1
#define ME 0
#define NEXT 1
#define MAXPROCESS 100

/************************************************************************
 * This program simulates a token ring communication. It will take an
 * user input message and destination, and circulate the token to its
 * destination. Upon message reaches its destination, the token is
 * pass back to root process then empty token continue circulates.
 *
 * @author Cheng Li
 * @version October 2, 2018
 ***********************************************************************/

/* Handles the signal. */
void sigHandler (int sigNum);

/* Token structure. */
struct token{
    int flag;   /* Flag to ensure the right people holds. */
    int dest;   /* Destination of the token. */
    char mes[MAX-8];    /* Token message. */
};

/* Size of the network. */
int size;

/* Define an integer to specify your position. */
int process = 0;

/* An array holds all pipe descriptors. */
int fd[MAXPROCESS][2];

/* A pid tracks child and my pid, not needed, only used it for testing. */
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
    
    /* Initialize enough pipe for the ring structure. */
    int i = 0;
    while(i < size){
        printf("Initialize pipes %d\n", i);
        if (pipe (fd[i]) < 0) {
            perror ("Pipe plumbing problem");
            exit(1);
        }
        i++;
    }
    
    int rootPID = getpid();
    
    /*
     * Loop to spawn declared size of process.
     * One parent will only spawn one child, then the child become
     * parent and spawn another child.
     * So it will be Parent->Child->Grandchild->Great Grandchild->etc..
     */
    i = 0;
    while(i < size){
        /* Run into an error when forking */
        if ((pid[NEXT] = fork()) < 0) {
            perror ("fork failed");
            exit(1);
        }
        
        /* You are actually the parent, don't need to fork again, so break. */
        if(pid[NEXT]){
            break;
        }
        
        /* Yes, you are the child. */
        if(!pid[NEXT]){
            /* Increment by 1 on each child spawn. */
            i++;
            process= i;
            /* Break, have enough process running */
            if(i == size-1){
                break;
            }
        }
    }
    
    /* Know who you are. */
    pid[ME] = getpid();
    printf("Pid %d Process %d\n", pid[ME], process);
    /* Setting the NEXT process for the last process. */
    if(process == (size-1)){
        pid[NEXT] = rootPID;
    }

    /* Signals an interrupt. */
    signal(SIGINT, sigHandler);
    
    /* Close all pipes not related to current process. */
    int closePos = 0;
    while(closePos < size){
        // If the pipe is the nearby two pipe.
        if(closePos == process || closePos == (process+size-1)%size){
            break;
        }
        close(fd[closePos][READ]);
        close(fd[closePos][WRITE]);
        closePos++;
    }
    
    /* Close read on writing side write on sending side. */
    close(fd[process][READ]);
    close(fd[(process+size-1)%size][WRITE]);
    
    /* The root process. */
    if(!process){
        /* Initialize a token structure. */
        struct token myToken;
        myToken.flag = 0;
        
        /*Sleep for two second to wait for all process to initialize. */
        sleep(1);
        
        /* User input for message. */
        char buffer[MAX];
        printf("Enter a message: ");
        if(fgets(buffer,sizeof(buffer),stdin) == NULL){
            perror("Failed to get user input");
            exit(1);
        }
        
        /* User input destination. */
        char dest[128];
        printf("Enter a destination: ");
        if(fgets(dest,sizeof(dest),stdin) == NULL){
            perror("Failed to get user input");
            exit(1);
        }
        
        int destination = atoi(dest);
        
        /* Putting message and destination into token. */
        myToken.dest = destination;
        strcpy(myToken.mes, buffer);
        
        printf("\nInitial token at process %d PID is %d\n", process, getpid());
        
        /* In case if you are sending message to your self. */
        if(myToken.dest == process){
            sleep(1);
            printf("\nA message for process %d: %s", process, myToken.mes);
            printf("Cleaning token information field...\n");
            /* Remove message and set destination to -1.. */
            strcpy(myToken.mes, "");
            myToken.dest = -1;
        }
        
        printf("Passing token..\n\n");
        sleep(2);
        /* Pass the token to next process. */
        myToken.flag+=1;
        write(fd[process][WRITE], (struct token*) &myToken, MAX);
        kill(pid[NEXT], SIGUSR1);
    }

    /* Each process Loop and wait for signal. */
    while(1){
        signal(SIGUSR1,sigHandler);
        pause();
    }
}

/************************************************************************
 * This function handles signal SIGINT and terminates the processes.
 * It waits for all child to exit and close all file descriptor upon exiting
 ***********************************************************************/
void sigHandler (int sigNum)
{
    if(sigNum == SIGUSR1){
        /* String buffer to receive message. */
        char str[MAX];
        
        /* A struct to convert message to token format. */
        struct token *tok;
        /* The pipe in array we read from. */
        int readPipe = (process+size-1)%size;
        int strNum = read(fd[readPipe][READ], (struct token*) str, MAX);
        if (strNum > MAX) {
            perror ("pipe read error\n");
            exit(1);
        }
        /* Caste received message into struct token format. */
        tok = (struct token *) str;
        // Holding the token.
        if(tok->flag == process){
            printf("Token passed to process %d PID is %d cpid%d\n", process, getpid(), pid[NEXT]);
            
            // Desination is the root, cleaning token message field.
            if(tok->dest == 0){
                printf("Cleaning token information field...\n\n");
                printf("Passing token..\n");
                /* Remove message and set destination to -1.. */
                strcpy(tok->mes, "");
                tok->dest = -1;
            }
            /**/
            else if(tok->dest == process){
                tok->dest=0;
                printf("A message for process %d: %s\n", process, tok->mes);
            }else{
                printf("Don't need it, passing token.\n\n");
            }
        }
        /* Sleep for 1 second so we can see whats going on. */
        sleep(2);
        /* Move the token to the next flag. */
        tok->flag+=1;
        /* Set flag to zero if this is the last process in the structure. */
        if(tok->flag==size){
            tok->flag=0;
        }
        /* Pass the token to next process. */
        write(fd[process][WRITE], (struct token*) tok, MAX);
        kill(pid[NEXT], SIGUSR1);
    }
    if(sigNum == SIGINT){  /* Exit on interrupt. */
        int status;
        
        /* Close all pipes descriptor again to ensure completeness. */
        int closePos = 0;
        while(closePos < size){
            close(fd[closePos][READ]);
            close(fd[closePos][WRITE]);
            closePos++;
        }
        /* Wait for the child to finish first. */
        wait(&status);
        printf ("\nCleaning and shutting down process %d\n", process);
        // this is where shutdown code would be inserted
        sleep (1);
        exit(0);
    }
}





