// Kara Pelster
// CISC361 PA 4

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glob.h>
#include <sys/wait.h>
#include <dirent.h>
#include "sh.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <utmpx.h>
#include <pthread.h>

int makePipe(char *args[64], int numArgs);
char *redirection(char *args[64], int numArgs);
void *watchUser(void *user);
void newUser(char *user);
void offUser(char *user);
int makePipe(char *args[64], int numArgs);
int checkForPipe(char *args[64], int numArgs);

struct pathelement *dir_list, *tmp;

Users_t *userHead; // pthread
pthread_mutex_t threadLock;

char *prefix = NULL;
char *prompt;

void sig_handler(int sig)
{
        fprintf(stdout, "\n>");
        fflush(stdout);
}

int main(int argc, char **argv, char **envp)
{
        char buf[MAXLINE];
        char *arg[MAXARGS]; // an array of tokens
        char *ptr;
        char *pch;
        pid_t pid;
        char *cmd;
        int status, i, arg_no;
        int errnum, done;
        int prompt_allo = 1;   //int variable to know if prompt has been allocated/ needs to be freed
        int pipeFlag;          // flag for pipe. 0 if no, 1 if yes
        int backgroundprocess; //count of num of background processes
        char *file;
        int pipesuccess;
        int sigflag;
        int noclobber = 0;

        int redir;
        char *redirFile, *reSymbol;

        int watchUserInit = 0;
        pthread_t userThread;

        struct pathelement *dir_list, *tmp;

        //allocate and set prefix initially
        prefix = getcwd(NULL, 0);
        //allocate and set empty prompt
        prompt = (char *)malloc(1);
        strcpy(prompt, "");

        char *arg0;
        char *executable;
        char *stat;

        char *execargs;

        signal(SIGINT, sig_handler);
        signal(SIGTSTP, sig_handler);

        fprintf(stdout, "[%s] > ", prefix);
        free(prefix);
        fflush(stdout);

        while (1)
        {
                /****** CHECK COMMAND LINE ******/
                stat = fgets(buf, MAXLINE, stdin);
                // printf("%s", stat);

                sigflag = 0;
                int fid;
                redir = 0;

                /****** HANDLE ^D ******/
                // FIX THIS***
                if (stat == NULL)
                {
                        printf("^D\n");
                        goto nextprompt;
                        sigflag = 1;
                }

                /****** EMPTY COMMAND LINE ******/
                if (strlen(buf) == 1 && buf[strlen(buf) - 1] == '\n')
                {
                        //buf[strlen(buf) - 1] = 0;
                        goto nextprompt; // "empty" command line
                }

                /****** GET RID OF LAST CHARACTER '\n'******/
                if (buf[strlen(buf) - 1] == '\n')
                {
                        buf[strlen(buf) - 1] = 0; /* replace newline with null */
                        // parse command line into tokens (stored in buf)
                }

                /****** CREATE CHAR ARRAY FROM COMMAND LINE ******/
                arg_no = 0;
                pch = strtok(buf, " ");
                while (pch != NULL && arg_no < MAXARGS)
                {
                        arg[arg_no] = pch;
                        arg_no++;
                        pch = strtok(NULL, " ");
                }
                arg[arg_no] = (char *)NULL; // null last character

                arg0 = arg[0]; // define first argument because it is used a lot

                /* print tokens
                for (i = 0; i < arg_no; i++)
                  printf("arg[%d] = %s\n", i, arg[i]);
                */

                // if (arg0 == NULL) // "blank" command line
                //         goto nextprompt;

                /****** CHECK BACKGROUND PROCESSES ******/
                backgroundprocess = 0;
                if (strcmp(arg[arg_no - 1], "&") == 0)
                { // check last line to see if it is &
                        backgroundprocess = 1;
                        arg[arg_no - 1] = NULL; //get rid of "&"
                        arg_no--;               //decrement num of arguments
                }

                /****** CHECK PIPE ******/
                pipeFlag = checkForPipe(arg, arg_no); // check first for pipe symbol
                if (pipeFlag)
                {
                        pipesuccess = makePipe(arg, arg_no);
                        if (pipesuccess < 0)
                        {
                                // printf("pipe failed\n");
                                goto nextprompt;
                        }
                        else
                        {
                                goto nextprompt;
                        }
                }

                /****** CHECK REDIRECTION ******/
                if (redirection(arg, arg_no) != NULL)
                {
                        redir = 1; // set redirection flag to 1
                        reSymbol = redirection(arg, arg_no);
                        redirFile = arg[arg_no - 1]; // set redirFile to last argument
                }

                /****** BEGIN CHECKING FOR BUILT INS ******/
                
                /****** NO CLOBBER ******/
                if (strcmp(arg0, "noclobber") == 0)
                // default is 0
                {
                        printf("Executing built-in [%s]\n", arg0);
                        if (arg_no < 2)
                        {
                                if (noclobber == 0)
                                {
                                        // read/write open
                                        noclobber = 1;
                                        printf("%d\n", noclobber); // print noclobber
                                }
                                else if (noclobber == 1)
                                {
                                        noclobber = 0;
                                        printf("%d\n", noclobber); // print noclobber
                                        // if noclobber is 1, prints ?
                                }
                        }
                        else
                        {
                                fprintf(stderr, "too many arguments\n");
                        }
                }

                /****** WATCHUSER ******/
                else if (strcmp(arg0, "watchuser") == 0)
                {
                        // debug printing list of users
                        // Users_t *current = userHead;
                        // while (current != NULL)
                        // {
                        //         printf("%s\n", current->name);
                        //         current = current->next;
                        // }

                        if (arg_no < 2)
                        {
                                printf("not enough arguments\n");
                        }
                        else if (arg_no == 2)
                        {
                                printf("Executing built-in [%s]\n", arg0); // add user
                                if (watchUserInit == 0)
                                { // create new thread
                                        pthread_create(&userThread, NULL, watchUser, "Users");
                                        watchUserInit = 1;
                                }
                                pthread_mutex_lock(&threadLock);
                                newUser(arg[1]);
                                pthread_mutex_unlock(&threadLock);
                        }
                        else if (arg_no == 3)
                        { // remove user
                                printf("Executing built-in [%s %s]\n", arg0, arg[1]);
                                pthread_mutex_lock(&threadLock);
                                offUser(arg[2]);
                                pthread_mutex_unlock(&threadLock);
                        }
                        else
                        {
                                printf("too many arguments\n");
                        }
                }

                /****** PWD ******/
                else if (strcmp(arg0, "pwd") == 0)
                { // built-in command pwd
                        printf("Executing built-in [pwd]\n");
                        ptr = getcwd(NULL, 0);
                        printf("%s\n", ptr);
                        free(ptr);
                }
                /****** WHICH ******/
                else if (strcmp(arg0, "which") == 0)
                { // built-in command which
                        struct pathelement *p, *tmp;
                        char *cmd;

                        //if no arguments
                        if (arg[1] == NULL)
                        { // "empty" which
                                printf("which: Too few arguments.\n");
                                goto nextprompt;
                        }

                        //if there are arguments, continue
                        printf("Executing built-in [which]\n");

                        //create linked list
                        p = get_path();

                        //set return value of which to be first itme found
                        cmd = which(arg[1], p);
                        if (cmd)
                        {
                                printf("%s\n", cmd);
                                free(cmd);
                        }
                        else // argument not found
                                printf("%s: Command not found\n", arg[1]);

                        while (p)
                        { // free list of path values
                                tmp = p;
                                p = p->next;
                                free(tmp->element);
                                free(tmp);
                        }
                }

                /****** EXIT ******/
                else if (strcmp(arg0, "exit") == 0)
                {
                        //exit loop
                        printf("Executing built-in [exit]\n");
                        if (watchUserInit) // handle pthread if it exists
                        {
                                int cancel = pthread_cancel(userThread);
                                if (cancel != 0)
                                {
                                        printf("cancel thread failed\n");
                                }
                                pthread_join(userThread, NULL);
                        }

                        free(prompt);
                        break;
                }

                /****** WHERE ******/
                else if (strcmp(arg0, "where") == 0)
                {
                        //where

                        struct pathelement *p, *tmp;
                        char *cmd;

                        if (arg[1] == NULL)
                        { // "empty" where
                                printf("where: Too few arguments.\n");
                                goto nextprompt;
                        }

                        printf("Executing built-in [where]\n");
                        p = get_path();
                        /***/
                        tmp = p;
                        while (tmp)
                        { // print list of paths
                                printf("path [%s]\n", tmp->element);
                                tmp = tmp->next;
                        }
                        /***/
                        printf("path [%s]\n", p->element);

                        while (p)
                        { // free list of path values
                                tmp = p;
                                p = p->next;
                                free(tmp->element);
                                free(tmp);
                        }
                }

                /****** CD ******/
                else if (strcmp(arg0, "cd") == 0)
                {
                        char *dr;
                        if (arg[1] != NULL && arg[2] != NULL)
                        {
                                printf("too many arguments\n");
                                goto nextprompt;
                        }

                        if (arg[1] == NULL)
                        { // "empty" chdir goes to home directory
                                printf("Executing built-in [cd HOME]\n");
                                dr = getenv("HOME");
                                chdir(dr);
                                //free(dr);
                                //cmd = strcat(getcwd(NULL, 0), "] > ");
                                goto nextprompt;
                        }
                        if (strcmp(arg[1], "-") == 0)
                        { // cd to previous directory
                                printf("Executing built-in [cd -]\n");
                                chdir("..");
                                goto nextprompt;
                        }
                        else
                        {
                                printf("Executing built-in [cd %s]\n", arg[1]);
                                dr = arg[1];
                                chdir(dr);
                                goto nextprompt;
                        }
                }

                /****** LIST ******/
                else if (strcmp(arg0, "list") == 0)
                {
                        //list
                        DIR *dir;
                        struct dirent *cf;

                        if (arg[1] == NULL)
                        {
                                printf("Executing built-in [list]\n");
                                dir = opendir(".");
                                if (dir == NULL)
                                {
                                        printf("No files in directory\n");
                                        free(dir);
                                        goto nextprompt;
                                }
                                while ((cf = readdir(dir)))
                                {
                                        printf("%s\n", cf->d_name);
                                }

                                while ((cf = readdir(dir)))
                                {
                                        free(cf->d_name);
                                }

                                closedir(dir);

                                goto nextprompt;
                        }

                        int ct = 1;
                        printf("Executing built-in [list <args>]\n");
                        while (arg[ct] != NULL)
                        {
                                printf("\n%s: \n", arg[ct]);
                                dir = opendir(arg[ct]);
                                if (dir == NULL)
                                {
                                        printf("No files in directory\n");
                                        goto nextprompt;
                                }
                                while ((cf = readdir(dir)))
                                {
                                        printf("%s\n", cf->d_name);
                                }

                                closedir(dir);
                                free(dir);
                                free(cf);
                                ct++;
                        }

                        goto nextprompt;
                }

                /****** PID ******/
                else if (strcmp(arg0, "pid") == 0)
                {
                        printf("Executing built-in [pid]\n");
                        int pid = getpid();
                        printf("Process ID of shell: %d\n", pid);
                }

                /****** KILL ******/
                else if (strcmp(arg0, "kill") == 0)
                {
                        //kill
                        int k;
                        char *arg1 = arg[1];
                        int pid;
                        int done, errnum;

                        if (arg1 == NULL)
                        {
                                printf("too few arguments\n");
                                goto nextprompt;
                        }

                        if (atoi(arg1) < 0)
                        {
                                printf("Executing built-in [kill -sig pid]\n");

                                pid = atoi(arg[2]);
                                k = kill((pid_t)pid, arg[2][1]);
                                if (k == 0)
                                {
                                        printf("Process %d killed successfully.\n", pid);
                                }
                                else
                                {
                                        errnum = errno;
                                        fprintf(stderr, "Error killing process %d: %s\n", pid, strerror(errnum));
                                }

                                free(arg1);
                                goto nextprompt;
                        }
                        else
                        {
                                printf("Executing built-in [kill pid]\n");
                                pid = atoi(arg[1]);
                                k = kill((pid_t)pid, SIGTERM);
                                if (k == 0)
                                {
                                        printf("Process %d killed successfully.\n", pid);
                                }
                                else
                                {
                                        //printf("Process %d kill failed.\n", pid);
                                        errnum = errno;
                                        fprintf(stderr, "Error killing process %d: %s\n", pid, strerror(errnum));
                                }
                                goto nextprompt;
                        }
                }

                /****** PROMPT ******/
                else if (strcmp(arg0, "prompt") == 0)
                {
                        if (prompt_allo == 1)
                        {
                                free(prompt);
                                prompt_allo = 0;
                        }

                        if (arg[1] == NULL)
                        {
                                printf("Executing built-in [prompt]\n");
                                printf("input prompt prefix: ");
                                if (fgets(buf, MAXLINE, stdin) != NULL)
                                {
                                        prompt = (char *)malloc(strlen(buf) + 1);
                                        strcpy(prompt, buf);
                                        prompt[strlen(buf) - 1] = 0;
                                }
                        }
                        else
                        {
                                printf("Executing built-in [prompt %s]\n", arg[1]);
                                //prefix = strcat(arg[1], " [");
                                prompt = (char *)malloc(strlen(arg[1]) + 1);
                                strcpy(prompt, arg[1]);
                        }
                        prompt_allo = 1;
                        goto nextprompt;
                }

                /****** PRINTENV ******/
                else if (strcmp(arg0, "printenv") == 0)
                {
                        if (arg[1] == NULL)
                        {
                                printf("Executing built-in [printenv]\n");
                                extern char **environ;
                                int ct = 0;
                                while (environ[ct])
                                {
                                        printf("%s\n", environ[ct++]);
                                }
                                goto nextprompt;
                        }

                        if (arg[2] != NULL)
                        {
                                printf("Too many arguments.\n");
                                goto nextprompt;
                        }

                        printf("Executing built-in [printenv <arg>]\n");
                        char *ev = getenv(arg[1]);
                        printf("%s = %s\n", arg[1], ev);

                        goto nextprompt;
                }

                /****** SETENV ******/
                else if (strcmp(arg0, "setenv") == 0)
                {
                        if (arg[1] == NULL)
                        {
                                printf("Executing built-in [setenv]\n");
                                extern char **environ;
                                int ct = 0;
                                while (environ[ct])
                                {
                                        printf("%s\n", environ[ct++]);
                                }
                                goto nextprompt;
                        }
                        if (strcmp(arg[1], "PATH") == 0)
                        {
                                if (arg[2] == NULL)
                                {
                                        printf("PATH variable needs a second argument\n");
                                        goto nextprompt;
                                }
                                setenv(arg[1], arg[2], 1);
                                get_path(); //set new path
                        }

                        //cd works and goes to home correctly
                        // else if (strcmp(arg[1], "HOME") == 0)
                        // {
                        // }

                        else if (arg[1] != NULL && arg[2] == NULL)
                        {
                                done = setenv(arg[1], "", 1); //1 as third argument overwrites if arg[1] exits
                        }
                        else if (arg[1] != NULL && arg[2] != NULL && arg[3] == NULL)
                        {
                                done = setenv(arg[1], arg[2], 1); //1 as third argument overwrites if arg[1] exits
                        }

                        else if (arg[3] != NULL)
                        {
                                printf("Too many arguments.\n");
                                goto nextprompt;
                        }

                        if (done == 0)
                        {
                                printf("Environment variable %s = %s created.\n", arg[1], arg[2]);
                        }

                        else if (done == -1)
                        {
                                errnum = errno;
                                fprintf(stderr, "Error making variable: %s\n", strerror(errnum));
                        }
                }


                /********** EXTERNAL COMMANDS **********/

                /***** external command with /, ./, ../ 
                 *  checks for redireciton 
                 * *****/
                else if (strncmp(arg[0], "/", 1) == 0 || strncmp(arg[0], "./", 2) == 0 || strncmp(arg[0], "../", 3) == 0)
                {
                        file = arg[0];
                        if (access(arg[0], F_OK) == 0)
                        {
                                int pid = fork();
                                if (pid < 0)
                                {
                                        printf("fork error\n");
                                        goto nextprompt;
                                }
                                if (pid == 0)
                                {
                                        if (reSymbol != NULL)
                                        {
                                                if (strcmp(reSymbol, ">") == 0)
                                                {
                                                        if (noclobber)
                                                        {
                                                                printf("noclobber on, cannot edit file.\n");
                                                                goto nextprompt;
                                                        }
                                                        else
                                                        {
                                                                fid = open(redirFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
                                                                close(1);
                                                                dup(fid);
                                                                close(fid);
                                                        }
                                                }
                                                else if (strcmp(reSymbol, ">&") == 0)
                                                {
                                                        if (noclobber)
                                                        {
                                                                printf("noclobber on, cannot edit file.\n");
                                                                goto nextprompt;
                                                        }
                                                        else
                                                        {
                                                                fid = open(redirFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                                                                close(1);
                                                                close(2);
                                                                dup(fid);
                                                                dup(fid);
                                                                close(fid);
                                                        }
                                                }
                                                else if (strcmp(reSymbol, ">>") == 0)
                                                {
                                                        if (noclobber)
                                                        {
                                                                printf("noclobber on, cannot append to file.\n");
                                                                goto nextprompt;
                                                        }
                                                        else
                                                        {
                                                                if (access(redirFile, F_OK) == 0)
                                                                {
                                                                        fid = open(redirFile, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                }
                                                                else
                                                                {
                                                                        fid = open(redirFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                }
                                                                close(1);
                                                                dup(fid);
                                                                close(fid);
                                                        }
                                                }
                                                else if (strcmp(reSymbol, ">>&") == 0)
                                                {
                                                        if (noclobber)
                                                        {
                                                                printf("noclobber on, cannot append to file.\n");
                                                                goto nextprompt;
                                                        }
                                                        else
                                                        {
                                                                if (access(redirFile, F_OK) == 0)
                                                                {
                                                                        fid = open(redirFile, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                }
                                                                else
                                                                {
                                                                        fid = open(redirFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                }
                                                                close(1);
                                                                close(2);
                                                                dup(fid);
                                                                dup(fid);
                                                                close(fid);
                                                        }
                                                }

                                                else if (strcmp(reSymbol, "<") == 0)
                                                {
                                                        // noclobber irrelevant
                                                        fid = open(redirFile, O_RDWR | S_IRUSR | S_IWUSR, S_IRWXU);
                                                        close(0);
                                                        dup(fid);
                                                        close(fid);
                                                }
                                        }
                                        // execve(arg[0], arg, NULL);
                                        // printf("couldn't execute: %s", buf); // only prints if execve fails
                                        // exit(127);
                                        /* child */
                                        // an array of aguments for execve()
                                        char *execargs[MAXARGS];
                                        glob_t paths;
                                        int csource, j;
                                        char **p;

                                        if (arg[0][0] != '/' && strncmp(arg[0], "./", 2) != 0 && strncmp(arg[0], "../", 3) != 0)
                                        { // get absoulte path of command
                                                dir_list = get_path();
                                                cmd = which(arg[0], dir_list);
                                                if (cmd)
                                                {
                                                } //printf("Executing [%s]\n", cmd);
                                                else
                                                { // argument not found
                                                        printf("%s: Command not found\n", arg[1]);
                                                        goto nextprompt;
                                                }

                                                while (dir_list)
                                                { // free list of path values
                                                        tmp = dir_list;
                                                        dir_list = dir_list->next;
                                                        free(tmp->element);
                                                        free(tmp);
                                                }
                                                execargs[0] = malloc(strlen(cmd) + 1);
                                                strcpy(execargs[0], cmd); // copy "absolute path"
                                                free(cmd);
                                        }
                                        else
                                        {
                                                execargs[0] = malloc(strlen(arg[0]) + 1);
                                                strcpy(execargs[0], arg[0]); // copy "command"
                                        }

                                        j = 1;
                                        for (i = 1; i < arg_no; i++)
                                        { // check arguments
                                                if (strchr(arg[i], '*') != NULL)
                                                { // wildcard!
                                                        csource = glob(arg[i], 0, NULL, &paths);
                                                        if (csource == 0)
                                                        {
                                                                for (p = paths.gl_pathv; *p != NULL; ++p)
                                                                {
                                                                        execargs[j] = malloc(strlen(*p) + 1);
                                                                        strcpy(execargs[j], *p);
                                                                        j++;
                                                                }
                                                                globfree(&paths);
                                                        }
                                                        else if (csource == GLOB_NOMATCH)
                                                        {
                                                                execargs[j] = malloc(strlen(arg[i]) + 1);
                                                                strcpy(execargs[j], arg[i]);
                                                                j++;
                                                        }
                                                }
                                                else
                                                {
                                                        execargs[j] = malloc(strlen(arg[i]) + 1);
                                                        strcpy(execargs[j], arg[i]);
                                                        j++;
                                                }
                                        }
                                        execargs[j] = NULL;
                                        if (reSymbol)
                                        {
                                                // get rid of last two arguments if redirection..... gets rid of symbol and file redirected to
                                                execargs[j - 1] = NULL;
                                                execargs[j - 2] = NULL;
                                        }

                                        execve(file, execargs, NULL);
                                        printf("couldn't execute: %s", buf); // only prints if execve fails
                                        // exit(127);
                                        goto nextprompt;
                                }
                                else
                                {
                                        if (!backgroundprocess)
                                        { // foreground process
                                                if ((pid = waitpid(pid, &status, 0)) < 0)
                                                        printf("waitpid error");
                                        }
                                        else
                                        { // no waitpid if background
                                                backgroundprocess = 0;
                                        }
                                }
                        }
                        else
                        {
                                printf("File does not exist\n");
                        }
                }

                /***** external command with no /, ./., ../ ----- uses which to get path 
                 * checks for redirection *****/ 
                else
                {
                        file = which(arg[0], get_path());

                        if (file != NULL)
                        {
                                if (access(file, F_OK) == 0)
                                {
                                        int pid = fork();
                                        if (pid < 0)
                                        {
                                                printf("Fork error\n");
                                                goto nextprompt;
                                        }
                                        if (pid == 0)
                                        {
                                                if (reSymbol != NULL)
                                                {
                                                        if (strcmp(reSymbol, ">") == 0)
                                                        {
                                                                if (noclobber)
                                                                {
                                                                        printf("noclobber is on, cannot edit file\n");
                                                                        goto nextprompt;
                                                                }
                                                                else
                                                                {
                                                                        fid = open(redirFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
                                                                        close(1);
                                                                        dup(fid);
                                                                        close(fid);
                                                                }
                                                        }
                                                        else if (strcmp(reSymbol, ">&") == 0)
                                                        {
                                                                if (noclobber)
                                                                {
                                                                        printf("noclobber is on, cannot edit file\n");
                                                                        goto nextprompt;
                                                                }
                                                                else
                                                                {
                                                                        fid = open(redirFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
                                                                        close(1);
                                                                        close(2);
                                                                        dup(fid);
                                                                        dup(fid);
                                                                        close(fid);
                                                                }
                                                        }
                                                        else if (strcmp(reSymbol, ">>") == 0)
                                                        {
                                                                if (noclobber)
                                                                {
                                                                        printf("noclobber is on, cannot append to file\n");
                                                                        goto nextprompt;
                                                                }
                                                                else
                                                                {
                                                                        if (access(redirFile, F_OK) == 0)
                                                                        {
                                                                                fid = open(redirFile, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                        }
                                                                        else
                                                                        {
                                                                                fid = open(redirFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                        }
                                                                        close(1);
                                                                        dup(fid);
                                                                        close(fid);
                                                                }
                                                        }
                                                        else if (strcmp(reSymbol, ">>&") == 0)
                                                        {
                                                                if (noclobber)
                                                                {
                                                                        printf("noclobber is on, cannot append to file\n");
                                                                        goto nextprompt;
                                                                }
                                                                else
                                                                {
                                                                        if (access(redirFile, F_OK) == 0)
                                                                        {
                                                                                fid = open(redirFile, O_RDWR | O_APPEND, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                        }
                                                                        else
                                                                        {
                                                                                fid = open(redirFile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR, S_IRWXU);
                                                                        }
                                                                        close(1);
                                                                        close(2);
                                                                        dup(fid);
                                                                        dup(fid);
                                                                        close(fid);
                                                                }
                                                        }

                                                        else if (strcmp(reSymbol, "<") == 0)
                                                        {
                                                                // noclobber irrelevant
                                                                fid = open(redirFile, O_RDWR | S_IRUSR | S_IWUSR, S_IRWXU);
                                                                close(0);
                                                                dup(fid);
                                                                close(fid);
                                                        }
                                                }

                                                /* child */
                                                // an array of aguments for execve()
                                                char *execargs[MAXARGS];
                                                glob_t paths;
                                                int csource, j;
                                                char **p;

                                                if (arg[0][0] != '/' && strncmp(arg[0], "./", 2) != 0 && strncmp(arg[0], "../", 3) != 0)
                                                { // get absoulte path of command
                                                        dir_list = get_path();
                                                        cmd = which(arg[0], dir_list);
                                                        if (cmd)
                                                        {
                                                        } //printf("Executing [%s]\n", cmd);
                                                        else
                                                        { // argument not found
                                                                printf("%s: Command not found\n", arg[1]);
                                                                goto nextprompt;
                                                        }

                                                        while (dir_list)
                                                        { // free list of path values
                                                                tmp = dir_list;
                                                                dir_list = dir_list->next;
                                                                free(tmp->element);
                                                                free(tmp);
                                                        }
                                                        execargs[0] = malloc(strlen(cmd) + 1);
                                                        strcpy(execargs[0], cmd); // copy "absolute path"
                                                        free(cmd);
                                                }
                                                else
                                                {
                                                        execargs[0] = malloc(strlen(arg[0]) + 1);
                                                        strcpy(execargs[0], arg[0]); // copy "command"
                                                }

                                                j = 1;
                                                for (i = 1; i < arg_no; i++)
                                                { // check arguments
                                                        if (strchr(arg[i], '*') != NULL)
                                                        { // wildcard!
                                                                csource = glob(arg[i], 0, NULL, &paths);
                                                                if (csource == 0)
                                                                {
                                                                        for (p = paths.gl_pathv; *p != NULL; ++p)
                                                                        {
                                                                                execargs[j] = malloc(strlen(*p) + 1);
                                                                                strcpy(execargs[j], *p);
                                                                                j++;
                                                                        }
                                                                        globfree(&paths);
                                                                }
                                                                else if (csource == GLOB_NOMATCH)
                                                                {
                                                                        execargs[j] = malloc(strlen(arg[i]) + 1);
                                                                        strcpy(execargs[j], arg[i]);
                                                                        j++;
                                                                }
                                                        }
                                                        else
                                                        {
                                                                execargs[j] = malloc(strlen(arg[i]) + 1);
                                                                strcpy(execargs[j], arg[i]);
                                                                j++;
                                                        }
                                                }
                                                execargs[j] = NULL;

                                                if (reSymbol)
                                                {
                                                        // get rid of last two arguments if redirection..... gets rid of symbol and file redirected to
                                                        execargs[j - 1] = NULL;
                                                        execargs[j - 2] = NULL;
                                                }

                                                execve(file, execargs, NULL);
                                                printf("couldn't execute: %s", buf); // only prints if execve fails
                                                goto nextprompt;
                                        }
                                        else
                                        {
                                                if (!backgroundprocess)
                                                { // foreground process
                                                        if ((pid = waitpid(pid, &status, 0)) < 0)
                                                                printf("waitpid error");
                                                }
                                                else
                                                { // no waitpid if background
                                                        backgroundprocess = 0;
                                                }
                                        }
                                }
                                else
                                {
                                        printf("File does not exist\n");
                                }
                        }
                        else
                        {
                                printf("File is null\n");
                        }
                }

                // prevent zombie
                waitpid(pid, NULL, WNOHANG);

        nextprompt:
                //define prefix
                prefix = getcwd(NULL, 0);
                if (strcmp(prompt, "") == 0)
                {
                        fprintf(stdout, "[%s] > ", prefix);
                }
                else
                {
                        fprintf(stdout, "%s [%s] > ", prompt, prefix);
                }
                //free prefix for next time
                free(prefix);
                fflush(stdout);
        }
}

int makePipe(char *args[64], int numArgs)
{
        /*
        separates left and right processes and starts pipe
        */

        char **left = malloc(sizeof(char *));
        char **right = malloc(sizeof(char *));
        char *newpipe;
        int piperight, pipeleft, status, argument;

        int pfd[2];
        // newpipe = NULL;
        argument = 0;
        while (argument < numArgs)
        {
                // check for pipe
                if ((strcmp(args[argument], "|") == 0) || (strcmp(args[argument], "|&") == 0))
                {
                        // if pipe found, save pipe. will break out of while loop
                        newpipe = args[argument];
                        break;
                }
                left[argument] = args[argument];
                // printf("left: %s\n", *left);
                argument++;
        }

        int rArgument = 0; // "right argument"
        argument++;
        // printf("%d\n", numArgs);
        while (argument < numArgs)
        {
                right[rArgument] = args[argument];
                argument++;
                rArgument++;
        }

        pipe(pfd); // start new pipe

        if (pipe(pfd) == -1)
        {
                fprintf(stderr, "pipe failed\n");
                return -1;
        }

        // left process

        pipeleft = fork();
        if (pipeleft == -1)
        {
                fprintf(stderr, "fork failed\n");
                return -1;
        }

        else if (pipeleft == 0)
        {
                fprintf(stdout, "child: left will now run\n");
                if (strcmp(newpipe, "|&") == 0)
                {
                        close(2); // close stderr
                }
                close(1);
                dup(pfd[1]);   // copy write end
                close(pfd[0]); // close read
                close(pfd[1]); // close write

                char *cmd;
                if (left[0][0] != '/' && strncmp(left[0], "./", 2) != 0 && strncmp(left[0], "../", 3) != 0)
                { // get absoulte path of command
                        dir_list = get_path();
                        cmd = which(left[0], dir_list);
                }

                execve(cmd, left, NULL);
                printf("execve failed\n");
                return -1;
        }

        // right process
        piperight = fork();
        if (piperight == -1)
        {
                fprintf(stderr, "fork failed\n");
                return -1;
        }

        else if (piperight == 0)
        {
                fprintf(stdout, "child: right child will now run\n");
                if (strcmp(newpipe, "|&") == 0)
                {
                        close(2); // close stderr
                }

                close(0);
                dup(pfd[0]);   // copy read end
                close(pfd[0]); // close read
                close(pfd[1]); // close write

                char *cmd;
                if (right[0][0] != '/' && strncmp(right[0], "./", 2) != 0 && strncmp(right[0], "../", 3) != 0)
                { // get absoulte path of command
                        dir_list = get_path();
                        cmd = which(right[0], dir_list);
                }

                execve(cmd, right, NULL);
                printf("execve failed\n");
                return -1;
        }

        // Parent doesn't need the pipes
        close(pfd[0]);
        close(pfd[1]);

        fprintf(stdout, "parent: Parent will now wait for children to finish execution\n");

        // Wait for all children to finish
        while (wait(NULL) > 0)
                ;

        fprintf(stdout, "---------------------\n");
        fprintf(stdout, "parent: Children has finished execution, parent is done\n");

        free(left);
        free(right);

        return 1;
}

char *redirection(char *args[64], int numArgs)
{
        /**
         * looks for redirection symbol. returns if found, null otherwise
         **/

        int argument;
        char *current;
        for (argument = 0; argument < numArgs; argument++)
        {
                current = args[argument];
                if ((strcmp(current, ">") == 0) || (strcmp(current, ">&") == 0) || (strcmp(current, ">>") == 0) || (strcmp(current, ">>&") == 0) || (strcmp(current, "<") == 0))
                {
                        // return redirection symbol found
                        return current;
                }
        }
        // if none of the indicated redirections, then return null to indicate no redirectioon needed
        return NULL;
}

void *watchUser(void *user)
{

         /**
         * pthreads method to watch if user logs on to new tty
         **/

        struct utmpx *userinfo; // utmpx is login info
        Users_t *current = userHead;

        setutxent(); // access file entries
        while (userinfo = getutxent())
        {
                if (userinfo->ut_type == USER_PROCESS)
                {
                        pthread_mutex_lock(&threadLock);
                }
                while (current != NULL)
                {
                        if (strcmp(current->name, userinfo->ut_user) == 0)
                        {
                                printf("%s has logged on %s from %s\n", userinfo->ut_user, userinfo->ut_line, userinfo->ut_host);
                        }
                        current = current->next;
                }
        }
}

void newUser(char *user)
{

        /**
         * iterates through linked list of threads to add new user
         **/


        Users_t *newUser = malloc(sizeof(struct Users));
        Users_t *current;
        struct utmpx *userinfo; // utmpx is login infos
        setutxent();            // access file entries
        userinfo = getutxent();
        newUser->name = malloc(strlen(user) + 1);
        strcpy(newUser->name, user);
        newUser->next = NULL;
        newUser->prev = NULL;

        // case if list is empty
        if (userHead == NULL)
        {

                userHead = newUser;
                userHead->prev = NULL;
                userHead->next = NULL;
        }
        // otherwise begin iterating
        else
        {
                current = userHead;
                while (current->next != NULL)
                {
                        current = current->next;
                }
                current->next = newUser; // when we get to last user, append new user
                newUser->prev = current; // prev of new user as current node
        }
}

void offUser(char *user)
{
        /**
         * iterates through linked list of threads to remove user. prints error message if user not found
         **/

        int exists = 0;
        Users_t *current;
        if (userHead == NULL)
        {
                printf("List of users empty. No users to remove.\n");
        }
        else
        {
                current = userHead;
                while (current != NULL)
                {
                        if (strcmp(current->name, user) == 0)
                        {
                                exists = 1;
                                //edge case : if user is first in list... just remove and set next node as userHead
                                if (current == userHead)
                                {
                                        userHead = userHead->next;
                                        free(current->name);
                                        free(current);
                                }
                                else if (current->prev != NULL)
                                {
                                        if (current->next != NULL)
                                        {
                                                // skip current. direct next's previous to be previous node
                                                current->next->prev = current->prev;
                                                // skip current. direct previous's next to be current next
                                                current->prev->next = current->next;
                                        }
                                        else
                                        {
                                                // if current is last node in list
                                                current->prev->next = NULL;
                                        }
                                        free(current->name);
                                        free(current);
                                }
                        }
                        else
                        {
                                current = current->next;
                        }
                }
        }
        if (!exists)
        {
                printf("user %s not found\n", user);
        }
}

int checkForPipe(char *arg[64], int numArgs)
{
        /*
        * Recognizes pipe symbols | and |& 
        * returns 0 if pipe symbol not found, 1 if found
        */

        int argument = 0;
        int pipefound = 0;
        while (argument < numArgs)
        {
                // check for pipe
                if ((strcmp(arg[argument], "|") == 0) || (strcmp(arg[argument], "|&") == 0))
                {
                        pipefound = 1;
                }
                argument++;
        }
        return pipefound;
}
