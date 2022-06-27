#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>

#define default_cut 64
#define default_sep " \t\r\n\a"
#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

struct job 
{
    int psid; //job process id
    int pstatus; //job status | running 1 | Stopped 0 | terminated -1
    char pcmd[1024];
};


char builtinCmd(char *);
char **cutCmd(char *thiscmd);
static int runIt(char** args, int format, int mode);
char* rmspace(char* s);
void printjob();
void addjob(int job_pid, int job_status, char cmd[]);
void clearjob();
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void fg_a_job(int jobnum ,int pid);

//initialize some variable for joblist
struct job joblist[1024];
int currjob = 1;
int currpid = -1;
int currjob_num = -1;


//main function
int main(int argc, char** argv)
{
    signal(SIGINT,sigint_handler);
    signal(SIGTSTP,sigtstp_handler);

    int form;
    char* lstemp[] = {"/bin/sh","ls",NULL};
    char thiscmd[1024];
    char thatcmd[1024];
    char temp[100];
    char** args;
    char b = 'F';
    char* p;

    strcpy(temp,"Home");

    while(1)
    {
        memset(thiscmd,0,1024);
        printf("> ");
        fgets(thiscmd,1000,stdin);
        thiscmd[strlen(thiscmd)-1] = 0;

        //printf("user input: %s\n", thiscmd);
        if((int)strlen(thiscmd)>1)
        {
          b = builtinCmd(thiscmd);
        }
        

        if(b == 'E') 
        {
          clearjob();
          return 0;
        }
        switch(b)
        {
            case 1:
                if(chdir(thiscmd+3) != 0)
                {
                    printf("../%s/: %s\n", thiscmd+3, strerror(errno));
                }
                getcwd(temp,99);
                break;
            
            case 2:
                system("ls");
                break;
            
            case 3:
                if(currjob == 1) // no jobs has been added
                {
                  printf("Nothing\n");
                }
                else
                {
                  printjob();
                }
                break;

            case 4: // run a suspended job in the background : bg
                args = cutCmd(rmspace(thiscmd));
                currjob_num = atoi(args[1]+1);
                if(currjob_num >= currjob || currjob_num == -1)
                {
                  printf("Job <%d> doesnt exist\n", currjob_num);
                }
                else if(joblist[currjob_num].pstatus == 1 || joblist[currjob_num].pstatus == -1)
                {
                  printf("Job <%d> is still running or terminated\n", currjob_num);
                }
                else
                {
                  joblist[currjob_num].pstatus = 1;
                  kill(joblist[currjob_num].psid, SIGCONT);
                  
                }

                currjob_num = -1;
                free(args);
                break;

            case 5:  // run a suspended job in the foreground : fg
                args = cutCmd(rmspace(thiscmd));
                currjob_num = atoi(args[1]+1);
                if(currjob_num >= currjob || currjob_num == -1)
                {
                  printf("Job <%d> doesnt exist\n", currjob_num);
                }
                else if(joblist[currjob_num].pstatus == 1 || joblist[currjob_num].pstatus == -1)
                {
                  printf("Job <%d> is still running or terminated\n", currjob_num);
                }
                else
                {
                  joblist[currjob_num].pstatus = 1;
                  fg_a_job(currjob_num,joblist[currjob_num].psid);
                  
                }

                currjob_num = -1;
                free(args);
                break;

            case 6:
                args = cutCmd(rmspace(thiscmd));
                currjob_num = atoi(args[1]+1);
                if(currjob_num >= currjob || currjob_num == -1)
                {
                  printf("Job <%d> doesnt exist\n", currjob_num);
                }
                else if(joblist[currjob_num].pstatus == -1)
                {
                  printf("Job <%d> is terminated already\n", currjob_num);
                }
                else
                {
                  joblist[currjob_num].pstatus = -1;
                  kill(joblist[currjob_num].psid,SIGKILL);
                  printf("[%d] %d Terminated %s\n", currjob_num, joblist[currjob_num].psid, joblist[currjob_num].pcmd);
                }

                currjob_num = -1;
                free(args);
                break;


            case 7: //run program in background
                //remove the & from last and remove space of front
                strncpy(thatcmd,thiscmd,(int)strlen(thiscmd)-1);
                args = cutCmd(rmspace(thatcmd));

                //know what path is it
                form = 0;
                if(!strncasecmp(args[0],"./",2))
                {
                  form = 2;
                }
                else if (!strncasecmp(args[0],"..",2))
                {
                  form = 1;
                }
                else
                {
                  form = -1;
                }
                
                //record current job to joblist
                runIt(args,form,0);
                printf("[%d] %d\n", currjob-1, joblist[currjob-1].psid);
                
                form = 0;
                free(args);
                break;

            case 8: //relative path format = 1 or 2
                args = cutCmd(rmspace(thiscmd));
                form = 1;
                if(!strncasecmp(args[0],"./",2))
                {
                  form = 2;
                }
                runIt(args,form,1);

                form = 0;
                free(args);
                break;

            case 9: //absolute path format = 0
                args = cutCmd(rmspace(thiscmd));
                runIt(args,0,1);


                free(args);
                break;
            
            case 0:
                args = cutCmd(rmspace(thiscmd));
                runIt(args,-1,1);


                free(args);
                break;
                
        }

    }
}


//read the first few letters to know what to do next
char builtinCmd(char *s)
{
    //remove the first few spaces
    char precmd[1024];
    strcpy(precmd,rmspace(s));

    //return cases
    if(!strcasecmp(precmd,"exit")) return 'E';
    else if(!strncasecmp(precmd,"jobs",4)) return 3;
    else if(!strncasecmp(precmd,"kill",4)) return 6;
    else if(!strncasecmp(precmd,"bg",2)) return 4;
    else if(!strncasecmp(precmd,"fg",2)) return 5;
    else if(precmd[(int)strlen(precmd)-1] == '&') return 7;
    else if(!strncasecmp(precmd,"cd",2)) return 1;
    else if(!strncasecmp(precmd,"ls",2)) return 2;
    else if(!strncasecmp(precmd,"./",2)) return 8;
    else if(!strncasecmp(precmd,"..",2)) return 8;
    else if(!strncasecmp(precmd,"/",1)) return 9;
    else return 0;
}

char* rmspace(char* s)
{
  int skip;
  int f_skip;
  for (skip = 0;skip<(int)strlen(s);skip++)
  {
    if(s[skip] == ' ')
    {
      skip++;
    }
    else
    {
      break;
    }
  }

  if(skip == 0)
  {
    f_skip = 0;
  }
  else
  {
    f_skip = skip-1;
  }
  return s+f_skip;
}

//cut the input into sections
char **cutCmd(char *thiscmd)
{
  int bufsize = default_cut;
  int pos = 0;
  char **output = malloc(bufsize * sizeof(char*));
  char *temp;

  if (!output)
  {
    fprintf(stderr, "cutCmd output malloc error\n");
    exit(EXIT_FAILURE);
  }

  temp = strtok(thiscmd, default_sep);

  while (temp != NULL) 
  {
    output[pos] = temp;
    pos++;

    if (pos >= bufsize) 
    {

      bufsize += default_cut;
      temp = realloc(output, bufsize * sizeof(char*));

      if (!output) 
      {
        fprintf(stderr, "realloc output error IN cutCmd\n");
        exit(EXIT_FAILURE);
      }

    }
    temp = strtok(NULL, default_sep);
  }

  output[pos] = NULL;
  return output;
}

//execute a program
/*-----------------------------------------
format -1 = find command from usr/bin or /bin
format 0 = absolute path
format 1 = relative path with ../
format 2 = relative path with ./
mode 1 = foreground
mode 0 = background
----------------------------------------------*/
static int runIt(char** args, int format, int mode)
{
  pid_t thispid;
  int status;
  pid_t waitthispid;
  char* filename;
  pid_t listen_child;

  bool terminated;
  bool stopped;
  

  char usr[512] = "/usr/bin/";
  char bin[512] = "/bin/";
  strncat(usr,args[0],sizeof(args[0]));
  strncat(bin,args[0],sizeof(args[0]));

  //recognize the input format
  if(format == 0)
  {
    filename = args[0];
  }
  else if(format == 1)
  {
    filename = args[0]+3;
  }
  else if(format == 2)
  {
    filename = args[0]+2;
  }
  
  //execute
  if((thispid = fork()) == 0)
  {
    setpgrp();
    
    //child process
    if(format == -1)
    {
      if(execve(usr,(char**)args,NULL) == -1)
      {
        if(execve(bin,(char**)args,NULL) == -1)
        {
          printf("%s: command not found\n",args[0]);
          
          exit(EXIT_FAILURE);
        }
      }
    }else
    {
      if(execve(filename,(char**)args,NULL) == -1)
      {
        perror("excute failed");
        
        exit(EXIT_FAILURE);
      }
    } 
  }
  else if (thispid<0)
  {
    perror("fork failed");
    
    exit(EXIT_FAILURE);
  }
  else
  {
    //parent process
    if(mode == 1) //foreground job, wait
    {
      currpid = thispid;
      
      do
      {
      waitthispid = waitpid(thispid,&status,WUNTRACED);
      } 
      while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

      terminated = WIFSIGNALED(status);
      stopped = WIFSTOPPED(status);
      if(WIFSIGNALED(status))
      {
        printf("child %d terminated with exit status %d\n", waitthispid, WTERMSIG(status));
      }
      else if(WIFSTOPPED(status))
      {
        printf("child %d Stopped with exit status %d\n", waitthispid, WSTOPSIG(status));
      }


    } 
    if(mode == 0) //background job, dont wait
    {
      waitthispid = waitpid(thispid,&status,WNOHANG);
    }
  }
  

  if(waitthispid == 0 || waitthispid == thispid) // when child run successful and return normally
  {
    if(status != 256) // if run at least
    {
      if(!stopped && !terminated) // running in background
      {
        addjob(thispid,1,filename); //set running
        
      }
      else if(stopped)
      {
        addjob(waitthispid,0,filename); // set Stopped
      }
      else
      {
        addjob(waitthispid,-1,filename); //set terminated
      }
    }
  }
  else if(waitthispid == -1)
  {
    perror("Child Error ");
  }


  return 1;
}

//print job for command jobs
void printjob()
{
    for(int k = 1;k<currjob;k++)
      {
        if(joblist[k].pstatus != -1)
        {
          printf("[%d] %d ", k, joblist[k].psid);
          if(joblist[k].pstatus==0)
          {
            printf("Stopped %s\n", joblist[k].pcmd);
          }
          else if(joblist[k].pstatus==-1)
          {
            printf("Terminated %s\n", joblist[k].pcmd);
          }
          else
          {
            printf("Running %s\n", joblist[k].pcmd);
          }
        }
        
      }
}
/*--------------------------------------------
job_pid is job's process id
job_status = 1 : Running
job_status = 0 : Stopped
cmd[] : record command
-----------------------------------------------*/
void addjob(int job_pid, int job_status, char cmd[])
{
    joblist[currjob].psid = job_pid;
    joblist[currjob].pstatus = job_status;
    strcpy(joblist[currjob].pcmd,cmd);
    
    currjob++;
}

void clearjob()
{
  for(int k = 0;k<currjob;k++)
  {
    if(joblist[k].pstatus == 1)
    {
      int j;
      j = kill(joblist[k].psid,SIGHUP);
    }
    else if(joblist[k].pstatus == 0)
    {
      int j;
      j = kill(joblist[k].psid,SIGCONT);
      j = kill(joblist[k].psid,SIGHUP);
    }
  }
}

void sigint_handler(int sig)
{
  //if there are some jobs and those jobs are still running
  int ret;
  if(currpid != -1) //there are some foreground job running
  {
    ret = kill(currpid,SIGINT);
    //printf("sent process %d a SIGINT\n",currpid);
    currpid = -1;
    printf("\n");
  }
  else
  {
    printf("no foreground job\n");
  }
}

void sigtstp_handler(int sig)
{
  int ret;
  if(currpid != -1)
  {
    ret = kill(currpid,SIGTSTP);
    currpid = -1;
    printf("\n");
  }
  else
  {
    printf("no foreground job\n");
  }
}



void fg_a_job(int jobnum ,int pid)
{
  
  currpid = joblist[jobnum].psid;
  //printf("edited currpid: %d   old currpid: %d\n",currpid,-1);

  pid_t wait_pid;
  int status;
  bool stopped;
  bool terminated;
  kill(pid,SIGCONT);

  do
  {
    wait_pid = waitpid(pid,&status, WUNTRACED);
  } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));
  terminated = WIFSIGNALED(status);
  stopped = WIFSTOPPED(status);

        if(WIFSIGNALED(status))
      {
        printf("child %d terminated with exit status %d\n", wait_pid, WTERMSIG(status));
      }
      else if(WIFSTOPPED(status))
      {
        printf("child %d Stopped with exit status %d\n", wait_pid, WSTOPSIG(status));
      }
  
      if(terminated) // running in background
      {
        joblist[jobnum].pstatus = -1; //set terminated
        
      }
      else if(stopped)
      {
        joblist[jobnum].pstatus = 0; // set Stopped
      }

  currpid = -1;
  //printf("fg finished, currpid: %d\n", currpid);
}


