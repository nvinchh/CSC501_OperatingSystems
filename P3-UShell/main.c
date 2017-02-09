/******************************************************************************
 *
 *  File Name........: main.c
 *
 *  Description......: Simple driver program for ush's parser
 *
 *  Author...........: Vincent W. Freeh
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <signal.h>
#include "parse.h"
#include <errno.h>

extern char **environ;
int initInputDesc, initOutputDesc, initErrorDesc;
int first, abortPipe = 0;
char *host;
int pipeFD[2], pipeFD2[2];

void handle_init(int signo)
{
  printf("\r\n"); 
  if(first)printf("%s%% ",host);
    fflush(STDIN_FILENO);
}
void handle_term(int signo)
{
    killpg(getpgrp(),SIGTERM);
    exit(0);
}
void handle_quit(int signo)
{
  printf("\r\n"); 
  if(first)printf("%s%% ",host);
    fflush(STDIN_FILENO);
}

static void prCmd(Cmd c)
{
  //printf("%s %d arguments\n", c->args[0], c->nargs);
  int i;
  int redirOpen;
  int isPipe = 0, isErrPipe = 0;


  if ( c )
  {
    //printf("%s%s ", c->exec == Tamp ? "BG " : "", c->args[0]);

    // Input redirection from the file pointed to by c->infile
    if ( c->in == Tin )
    {
      //printf("<(%s) ", c->infile);
      redirOpen = open(c->infile, O_RDONLY, 0600);

      if(redirOpen < 0)
      {
        printf("%s: No such file/directory\n", c->infile);
        exit(0);
      }
      else
      {
        if(dup2(redirOpen, STDIN_FILENO) < 0)
        {
          perror("Error in dup2");
          exit(-1);
        }
      }
    }

    if ( c->out != Tnil )
      switch ( c->out )
      {
        case Tout:
          //printf(">(%s) ", c->outfile);
          redirOpen = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0660);
          if(redirOpen < 0)
          {
            printf("%s: Error in creating/opening file\n", c->outfile);
            exit(1);
          }
          else
          {
            if(dup2(redirOpen, STDOUT_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
          }
          break;

        case Tapp:
          //printf(">>(%s) ", c->outfile);
          redirOpen = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0660);
          if(redirOpen < 0)
          {
            printf("%s: Error in creating/opening file \n", c->outfile);
            exit(1);
          }
          else
          {
            if(dup2(redirOpen, STDOUT_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
          }
          break;

        case ToutErr:
          //printf(">&(%s) ", c->outfile);
          redirOpen = open(c->outfile, O_WRONLY|O_CREAT|O_TRUNC, 0660);
          if(redirOpen < 0)
          {
            printf("%s: Error in creating/opening file \n", c->outfile);
            exit(1);
          }
          else
          {
            if(dup2(redirOpen, STDOUT_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
            if(dup2(redirOpen, STDERR_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
          }
          break;

        case TappErr:
          //printf(">>&(%s) ", c->outfile);
          redirOpen = open(c->outfile, O_WRONLY|O_CREAT|O_APPEND, 0660);
          if(redirOpen < 0)
          {
            printf("%s: Error in creating/opening file", c->outfile);
            exit(1);
          }
          else
          {
            if(dup2(redirOpen, STDOUT_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
            if(dup2(redirOpen, STDERR_FILENO) < 0)
            {
              perror("Error in dup2");
              exit(-1);
            }
          }
          break;
        case Tpipe:
          //printf("| ");
          isPipe = 1;
          break;
        case TpipeErr:
          //printf("|& ");
          isErrPipe = 1;
          break;

        default:
          fprintf(stderr, "Shouldn't get here\n");
          exit(-1);
      }

      // cd
      if(strcmp(c->args[0], "cd")==0)
      {
        // if only cd is typed then changing to home directory
        if(c->nargs == 1)
        {
          chdir(getenv("HOME"));
        }
        else
        {
          if(chdir(c->args[1])<0)
          {
            printf("%s: No such file or directory\n", c->args[1]);
          }
        }
      }
      //echo
      else if(strcmp(c->args[0], "echo")==0)
      {
        for(i=1; c->args[i]!=NULL; i++)
        {
          if(fputs(c->args[i], stdout)!=EOF)
          {
            fputs(" ", stdout);
          }
        }
        putchar('\n');
      }
      //logout
      else if(strcmp(c->args[0], "logout")==0)
      {
        exit(0);
      }

      //nice
      else if(strcmp(c->args[0], "nice")==0)
      {
        //printf("Inside nice\n");
        int who = getpid();
        if(c->nargs == 1)
        {
          //printf("1 arg\n");
          setpriority(PRIO_PROCESS, who, 4);
        }
        else if(c->nargs == 2)
        {
          //printf("2 args\n");
          int length = strlen(c->args[1]);
          if(isdigit(c->args[1][length-1]))
          {
            setpriority(PRIO_PROCESS, who, atoi(c->args[1]));
          }
          else
          {
            
            pid_t forkID = fork();
            if(forkID < 0)
              perror("Failed to fork");
            else if(forkID == 0)
            {
              setpriority(PRIO_PROCESS, getpid(), 4);
              execvp(c->args[1], &c->args[1]);
            }
            else
              wait(NULL);
          }
        }
        else
        {
          //printf("else args\n");
          pid_t forkID = fork();
          if(forkID < 0)
            perror("Failed to fork");
          else if(forkID == 0)
          {
            int length = strlen(c->args[1]);
            int priority = 4;
            int check = 0;

            if(isdigit(c->args[1][length-1]))
            {
              //printf("Is priority true");
              priority = atoi(c->args[1]);
              check = 1;
            }

            setpriority(PRIO_PROCESS, getpid(), priority);
            if(check)
              execvp(c->args[2], &c->args[2]);
            else
              execvp(c->args[1], &c->args[1]);
          }
          else
            wait(NULL);
        }
        //printf("Finishing nice\n");
      }
      // pwd
      else if(strcmp(c->args[0], "pwd")==0)
      {
        char currDirectory[800];
        getcwd(currDirectory, 800);
        printf("%s \n", currDirectory);
      }
      //setenv
      else if(strcmp(c->args[0], "setenv")==0)
      {
        //printf("Inside setenv \n");
        char **env;
        if(c->nargs==1)
        {
          for(env = environ; *env!=NULL; env++)
          {
            printf("%s \n", *env);
          }
        }
        else
        {
          int environVarLength = strlen(c->args[1]);
          if(c->args[2]!=NULL)
            environVarLength += strlen(c->args[2]);
          char *temp = malloc((environVarLength+1)*sizeof(char));
          strcpy(temp, c->args[1]);
          strcat(temp, "=");

          if(c->args[2]!=NULL)
            strcat(temp, c->args[2]);
          else
            strcat(temp, "");
          putenv(temp);
        }
        //printf("Finishing setenv\n");
      }
      //unsetenv
      else if(strcmp(c->args[0], "unsetenv")==0)
      {
        //printf("Inside unsetenv\n");
        if(c->args[1]!=NULL)
        {
          char **env;
          for(env = environ; *env!=NULL; env++)
          {
            if(strncmp(*env, c->args[1], strlen(c->args[1]))==0 && (*env)[strlen(c->args[1])]== '=')
            {
              for(; *env!=NULL; env++)
              {
                *env = *(env+1);
              }
              break;
            }
          }
        }
        //printf("Finishing unsetenv\n"); 
      }
      else if(strcmp(c->args[0], "where")==0)
      {
        char *pathVal, *checkCommand;
        char **paths, **temporary;
        int pathCount = 0, i, j, foundPos = 0;
        pathVal = (char*)malloc(300);
        if(c->args[1]!=NULL)
        {
          for(temporary = environ; *temporary!=NULL; temporary++)
          {
            if(strncmp(*temporary, "PATH", 4)==0 && (*temporary)[4]=='=')
            {
              strcpy(pathVal, *temporary+5);
              break;
            }
          }
          char * tempPath = pathVal;
          while(strchr(pathVal, ':')!=NULL)
          {
            pathCount++;
            pathVal = strchr(pathVal, ':') + 1;
          }
          pathCount++;
          pathVal = tempPath;
          paths = malloc(sizeof(char*) * pathCount);
          i = 0;
          j = 0;

          while(i++ < strlen(pathVal))
          {
            if(pathVal[i] == ':')
            {
              paths[j] = malloc(sizeof(char) * (i - foundPos));
              if(foundPos == 0)
                paths[j++] = strndup(pathVal, i);
              else
                paths[j++] = strndup(pathVal + foundPos + 1, i - foundPos - 1);
              foundPos = i;
            }
          }

          if(foundPos == 0)
            paths[j++] = strndup(pathVal, i);
          else
            paths[j++] = strndup(pathVal + foundPos + 1, i - foundPos - 1);

          int exist;

          for(i = 1; i< c->nargs; i++)
          {
            j = 0;
            if((strcmp(c->args[i], "cd")==0 || strcmp(c->args[i], "echo")==0 || strcmp(c->args[i], "logout")==0 ||
              strcmp(c->args[i], "nice")==0 || strcmp(c->args[i], "pwd")==0 || strcmp(c->args[i], "setenv")==0 ||
              strcmp(c->args[i], "unsetenv")==0 || strcmp(c->args[i], "where")==0))
            {
              printf("%s is a shell built-in \n", c->args[i]);
            }

            int temp = pathCount;
            while(temp-->0)
            {
              checkCommand = malloc((strlen(paths[j]) + strlen(c->args[i]) + 1)* sizeof(char));
              strcpy(checkCommand, paths[j++]);
              strcat(checkCommand, "/");
              strcat(checkCommand, c->args[i]);

              if((exist = open(checkCommand, O_RDONLY, 0660)) > 0)
              {
                printf("%s \n", checkCommand);
                break;
              }
              free(checkCommand);
            }
          }
        }
        else
          printf("Syntax: where [command]\n"); 
      }
      else
      {
        //printf("Inside final else\n");
        int i, count = 0;
        for(i = 1; i< c->nargs; i++)
        {
          count += strlen(c->args[i]);
          //printf("size %d\n",count);
        }

        char **args = malloc(count * sizeof('a'));
        for(i=0; i< c->nargs; i++)
          args[i] = c->args[i];

        int containsSlash = 0, absPath = 0;
        char *temp = c->args[0];
        for(i=0; i< strlen(c->args[0]); i++)
        {
          if(temp[i]=='/')
          {
            containsSlash = 1;
            if(i == 0)
              absPath = 1;
            break;
          }
        }
        if(execvp(c->args[0], c->args)<0)
        {
          //printf("Inside if(execvp(c->args[0], c->args[0])<0)\n");
          //printf("%d errno\n", errno);
          if(errno == ENOENT)
            printf("%s Command not found \n", c->args[0]);
          if(errno == EACCES || errno == EISDIR)
            printf("%s Permission denied\n", c->args[0]);
          //printf("Just before exit\n");
          exit(-1);
          //printf("Just after exit\n");
        }
        //printf("Finishing final else\n");
      }

      if(isPipe || isErrPipe)
        exit(EXIT_SUCCESS);



      /*if ( c->nargs > 1 )
      {
        printf("[");
        for ( i = 1; c->args[i] != NULL; i++ )
          printf("%d:%s,", i, c->args[i]);
        printf("\b]");
      }
      putchar('\n');  */
    // this driver understands one command
      if ( !strcmp(c->args[0], "end") )
        exit(0);
  }
}

static void prPipe(Pipe p)
{
  int i = 0;
  Cmd c;
  int count = 0, status, w;
  initInputDesc = dup(STDIN_FILENO);
  initOutputDesc = dup(STDOUT_FILENO);
  initErrorDesc = dup(STDERR_FILENO);

  if ( p == NULL )
    return;

  //printf("Begin pipe%s\n", p->type == Pout ? "" : " Error");
  for ( c = p->head; c != NULL; c = c->next )
  {
    if(!strcmp(c->args[0], "end"))
      exit(0);
    if(abortPipe)
    {
      abortPipe = 0;
      break;
    }

    if(c->next!=NULL)
    {
      if(count == 0)
      {
        pipe(pipeFD);
        dup2(pipeFD[1], STDOUT_FILENO);

        if(c->out == TpipeErr)
          dup2(pipeFD[1], STDERR_FILENO);
      }
      else
      {
        pipe(pipeFD2);
        dup2(pipeFD2[1], STDOUT_FILENO);

        if(c->out == TpipeErr)
          dup2(pipeFD2[1], STDERR_FILENO);
      }
    }
    else
      dup2(initOutputDesc, STDOUT_FILENO);

    pid_t forkID = 0;

    if(!(c->next == NULL && (strcmp(c->args[0], "cd")==0 || strcmp(c->args[0], "echo")==0 || strcmp(c->args[0], "logout")==0
      || strcmp(c->args[0], "nice")==0 || strcmp(c->args[0], "pwd")==0 || strcmp(c->args[0], "setenv")==0 ||
      strcmp(c->args[0], "unsetenv")==0 || strcmp(c->args[0], "where")==0)))
    {
      forkID = fork();
      //printf("Inside fork\n");
    }

    if(forkID < 0)
    {
      perror("Error in fork");
      exit(0);
    }
    else if(forkID == 0)
    {
      if(c->out == Tpipe || c->out == TpipeErr)
        setpgid(0, getpgrp());
        prCmd(c);
    }
    else
    {
      waitpid(-1, &status, 0);
      if(status == 65280)
      {
        if(c->out == Tpipe || c->out == TpipeErr)
          abortPipe = 1;
      }
    }

    if(c->next!= NULL)
    {
      if(count == 0)
      {
        dup2(pipeFD[0],STDIN_FILENO);
        count++;
        dup2(initOutputDesc,STDOUT_FILENO);
        dup2(initErrorDesc,STDERR_FILENO);
        close(pipeFD[1]);
      }
      else
      {
        dup2(pipeFD2[0],STDIN_FILENO);
        count--;
        dup2(initOutputDesc,STDOUT_FILENO);
        dup2(initErrorDesc,STDERR_FILENO);
        close(pipeFD2[1]);
      }
    }
    else
      dup2(initOutputDesc, STDOUT_FILENO);
    

    //printf("  Cmd #%d: ", ++i);
    //prCmd(c);
  }
  fflush(stdout);

  if(dup2(initInputDesc, STDIN_FILENO) < 0)
  {
    perror("Error in dup2");
  }
  if(dup2(initOutputDesc, STDOUT_FILENO) < 0)
  {
    perror("Error in dup2");
  }
  if(dup2(initErrorDesc, STDERR_FILENO) < 0)
  {
    perror("Error in dup2");
  }


  //printf("End pipe\n");
  prPipe(p->next);
}

void loadingRC(Pipe p)
{
  const char* homeEnvVar="HOME";
  int openFileRet;

  //path to ushrc file
  char *homePath=malloc(sizeof(char*)*strlen(getenv("HOME"))); 

  strcpy(homePath,getenv(homeEnvVar));
  strcat(homePath,"/.ushrc");
  //printf("%s hello\n",homePath);
  int initInpSrc=dup(STDIN_FILENO);
  openFileRet=open(homePath,O_RDONLY,0600);
   
  if(openFileRet<0)
  {
    printf("Could not open ~/.ushrc file \n" );
  }
  else
  {
    if(!(dup2(openFileRet, STDIN_FILENO)<0))
    {
      p = parse();
      if(p!=NULL)
      {
        while(!(strcmp(p->head->args[0], "end")==0))
        {
          fflush(stdout);
          prPipe(p);
          fflush(stdout);
          freePipe(p);
          p = parse();

          if(p == NULL)
            break;
        }
      }
      close(openFileRet);
      if(dup2(initInpSrc, STDIN_FILENO)<0)
      {
        perror("Dup2 Error");
        exit(0);
      }
    }
  }

}

int main(int argc, char *argv[])
{
  Pipe p;
  //char *host = "nvinchh_ush";
  host=malloc(sizeof(char)*50);
  gethostname(host,400);

  signal(SIGTERM, handle_term);
  signal(SIGQUIT, handle_init);
  signal(SIGINT, handle_init);

  loadingRC(p);
  setpgid(0, 0);


  while ( 1 )
  {
    printf("%s%% ", host);
    fflush(stdout);
    
    first = 1;
    p = parse();
    //printf("%s ",p);
    first = 0;

    prPipe(p);
    //printf("Before Free Pipe\n");
    freePipe(p);
    //printf("After Free Pipe\n");
  }
}

/*........................ end of main.c ....................................*/
