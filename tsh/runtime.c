/***************************************************************************
 *  Title: Runtime environment
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.3 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.3  2009/10/12 20:50:12  jot836
 *    Commented tsh C files
 *
 *    Revision 1.2  2009/10/11 04:45:50  npb853
 *    Changing the identation of the project to be GNU.
 *
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "interpreter.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
bgjobL *oldest_bgjob = NULL;

const int JOB_RUNNING = 0;
const int JOB_STOPPED = 1;
const int JOB_DONE = 2;
/************Function Prototypes******************************************/

EXTERN void
freeCommand(commandT* cmd);

/* run command */
static void
RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void
RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool
ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void
Exec(commandT*, bool, bool, bool, int(*)[2]);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);

void
freeCommandTList(commandT_list* cmd_cell);

bool
is_bg(commandT *cmd);
int
job_stack_size();

int
push_bg_job(pid_t, commandT*);

bgjobL*
pop_bg_job(pid_t);

bgjobL *
delete_job_num(int job_num);

bgjobL *
continue_job_num(int job_num);


int
size_of_bgjobs(void);

/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
 * RunCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs the given command.
 */
	void
RunCmd(commandT* cmd)
{
	RunCmdFork(cmd, TRUE);
} /* RunCmd */


/*
 * RunCmdFork
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Runs a command, switching between built-in and external mode
 * depending on cmd->argv[0].
 */
	void
RunCmdFork(commandT* cmd, bool fork)
{
	if (cmd->argc <= 0)
		return;
	if (IsBuiltIn(cmd->argv[0]))
	{
		RunBuiltInCmd(cmd);
	}
	else
	{
		RunExternalCmd(cmd, fork);
	}
} /* RunCmdFork */


/*
 * RunCmdBg
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a command in the background.
 */
	void
RunCmdBg(commandT* cmd)
{
	// TODO
} /* RunCmdBg */


/*
 * RunCmdPipe
 *
 * arguments:
 *   commandT *cmd1: the commandT struct for the left hand side of the pipe
 *   commandT *cmd2: the commandT struct for the right hand side of the pipe
 *
 * returns: none
 *
 * Runs two commands, redirecting standard output from the first to
 * standard input on the second.
 */
    void
RunCmdPipe(commandT_list* commands)
{
    int old_fd[2];
    int **old_fd_ptr = &old_fd;
    commandT_list* top_cmd = commands;
    commandT_list* prev_cmd = NULL;
    while(top_cmd)
    {
        printf("top name: %s\n", top_cmd->cmd->name);
        Exec(top_cmd->cmd, TRUE, (top_cmd->next != NULL),
             (prev_cmd != NULL), old_fd_ptr);
        if(prev_cmd)
            freeCommandTList(prev_cmd);
        prev_cmd = top_cmd;
        top_cmd = top_cmd->next;
    }
    if(prev_cmd)
        freeCommandTList(prev_cmd);
} /* RunCmdPipe */


void
freeCommandTList(commandT_list* cmd_cell)
{
    freeCommand(cmd_cell->cmd);
    free(cmd_cell);
}

/*
 * RunCmdRedirOut
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard output
 *
 * returns: none
 *
 * Runs a command, redirecting standard output to a file.
 */
	void
RunCmdRedirOut(commandT* cmd, char* file)
{
} /* RunCmdRedirOut */


/*
 * RunCmdRedirIn
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   char *file: the file to be used for standard input
 *
 * returns: none
 *
 * Runs a command, redirecting a file to standard input.
 */
	void
RunCmdRedirIn(commandT* cmd, char* file)
{
}  /* RunCmdRedirIn */


/*
 * RunExternalCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool fork: whether to fork
 *
 * returns: none
 *
 * Tries to run an external command.
 */
	static void
RunExternalCmd(commandT* cmd, bool fork)
{
	if (ResolveExternalCmd(cmd))
		Exec(cmd, fork, FALSE, FALSE, NULL);
	else if(strcmp(cmd->name, "exit")!=0) //if exit, let interpreter handle it
	{
		Print("./tsh line 1: ");
		Print(cmd->name);
		Print(": ");
		Print("No such file or directory\n");
                freeCommand(cmd);
	}
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 ::*
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: bool: whether the given command exists
 *
 * Determines whether the command to be run actually exists.
 */
	static bool
ResolveExternalCmd(commandT* cmd)
{
	char *path;
	char *pathtoken;
	char attemptPath[256];
	if(FileExists(cmd->name)) //checks local and absolute path
		return TRUE;

	path = getenv("PATH");  //otherwise get path

	size_t const len = strlen(path)+1;
	char *copypath = memcpy(malloc(len), path, len); //copy it into new memory
	pathtoken = strtok(copypath,":"); //tokenize using : as delimiter

	while(pathtoken != NULL)
	{
		strncpy(attemptPath,pathtoken,63); //copy token into a tmp var
		strncat(attemptPath,"/",1); //add on a slash
		strncat(attemptPath,cmd->name,194); // and the command name
		if(FileExists(attemptPath)) //check for existence (absolute)
		{
			free(copypath); //if found, free and return
			return TRUE;
		}
		pathtoken = strtok(NULL, ":"); //otherwise continue to tokenize
	}
	free(copypath);
	return FALSE;
} /* ResolveExternalCmd */


/*
 * Exec
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *   bool forceFork: whether to fork
 *
 * returns: none
 *
 * Executes a command.
 */
static void
Exec(commandT* cmd, bool forceFork, bool next, bool prev,
     int (*old_fd_in)[2])
{
	int new_fd[2];
        int *old_fd = *old_fd_in;
	pid_t pid;
	char *path;
	char *pathtoken;
	char attemptPath[256];
	bool onPath = FALSE;
        bool make_bg = FALSE;
        if(is_bg(cmd))
            make_bg = TRUE;

        if(next)
            pipe(new_fd);

	if(!FileExists(cmd->name)) //if you cant find it locally, we need to go find it on the path
	{
		path = getenv("PATH");
		size_t const len = strlen(path)+1;
		char *copypath = memcpy(malloc(len), path, len);
		pathtoken = strtok(copypath,":");
		while(pathtoken != NULL) // same as before, this time
		{
			strncpy(attemptPath,pathtoken, 63);
			strncat(attemptPath,"/",1);
			strncat(attemptPath,cmd->name,194);
			if(FileExists(attemptPath))
			{
				onPath = TRUE;
				break;
			}
			pathtoken = strtok(NULL, ":");
		}
		free(copypath);
	}
	if(!onPath) //wasnt found on path, must be local/absolute
	{
		memset(attemptPath, 0, 256);
		memcpy(attemptPath, cmd->name,256);
	}

	if(forceFork) //if told to fork...
	{
            sigset_t sigs;
            sigemptyset(&sigs);
            sigaddset(&sigs, SIGCHLD);
            sigprocmask(SIG_BLOCK, &sigs, NULL);


            pid = fork(); //fork
            if(pid == 0) //Child
            {
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
                setpgid(0, 0);
                if(prev)
                {
                    dup2(old_fd[0], 0);
                    close(old_fd[0]);
                    close(old_fd[1]);
                }
                if(next)
                {
                    close(new_fd[0]);
                    dup2(new_fd[1], 1);
                    close(new_fd[1]);
                }
                execv(attemptPath, cmd->argv);
            }
            fg_pgid = pid;
            if (make_bg)
            {
                push_bg_job(pid, cmd);
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
            }
            else
            {
                sigprocmask(SIG_UNBLOCK, &sigs, NULL);
                if(prev)
                {
                    close(old_fd[0]);
                    close(old_fd[1]);
                }
                if(next)
                    *old_fd_in = new_fd;

                int Status;
                waitpid(pid, &Status, WUNTRACED);
                if (WIFSTOPPED(Status))
                {
                    fg_pgid = 0;
                    push_bg_job(pid, cmd);
                }
                else
                    freeCommand(cmd);
            }
        } else
            execv(attemptPath, cmd->argv); //exec without forking
        fg_pgid = 0;

} /* Exec */

int
push_bg_job(pid_t pid, commandT* cmd)
{
    bgjobL *job = malloc(sizeof(bgjobL));
    if(job)
    {
        job->pid = pid;
        job->cmd = cmd;
        job->next = bgjobs;
        job->prev = NULL;
        if (bgjobs != NULL)
        {
            job->start_position = bgjobs->start_position + 1;
            bgjobs->prev = job;
        }
        else
        {
            job->start_position = 1;
            oldest_bgjob = job;
        }
        bgjobs = job;
        setpgid(pid, pid);
        return 0;
    }
    return -1;
}

void
free_job(bgjobL* job)
{
    free(job);
}

bgjobL*
pop_bg_job(pid_t pid)
{
    bgjobL* prev_job = NULL;
    bgjobL* top_job = bgjobs;
    while(top_job != NULL)
    {
        if (pid == top_job->pid)
        {
            if (prev_job == NULL)
            {
                bgjobs = top_job->next; // first thing on stack
                if (top_job == oldest_bgjob)
                    oldest_bgjob = NULL;
            }
            else if(prev_job != NULL && top_job != oldest_bgjob)
            {
                // in the middle of the stack
                prev_job->next = top_job->next;
                top_job->next->prev = prev_job;
            }
            else
            {
                // last thing on stack and not first
                oldest_bgjob = top_job->prev;
                oldest_bgjob->next = NULL;
            }

            top_job->next = NULL;
            top_job->prev = NULL;
            return top_job;
        } else
        {
            prev_job = top_job;
            top_job = top_job->next;
        }
    }
    return NULL;
}

int
size_of_bgjobs(void)
{
    int size = 0;
    bgjobL* top_job = bgjobs;
    while(top_job != NULL){
        size++;
        top_job = top_job->next;
    }
    return size;
}



bool
is_bg(commandT *cmd)
{
    if (strcmp(cmd->argv[(cmd->argc)-1], "&") == 0)
    {
        // last arg is "&"
        free(cmd->argv[(cmd->argc)-1]);
        cmd->argv[(cmd->argc)-1] = 0;
        cmd->argc--;
        return TRUE;
    }
    int last_arg_len = strlen(cmd->argv[(cmd->argc)-1]);
    if (cmd->argv[(cmd->argc)-1][last_arg_len-1] == '&')
    {
        // last char of last arg is "&"
        cmd->argv[(cmd->argc)-1][last_arg_len-1] = '\0';
        return TRUE;
    }
    return FALSE;
}


/*
 * IsBuiltIn
 *
 * arguments:
 *   char *cmd: a command string (e.g. the first token of the command line)
 *
 * returns: bool: TRUE if the command string corresponds to a built-in
 *                command, else FALSE.
 *
 * Checks whether the given string corresponds to a supported built-in
 * command.
 */
	static bool
IsBuiltIn(char* cmd)
{
    // only built in we need to worry about as of now
    if(strcmp( cmd, "cd")==0 || strcmp(cmd, "jobs") == 0 || \
       strcmp(cmd, "fg") == 0 || (strcmp(cmd, "bg") == 0))
        return TRUE;
    return FALSE;
} /* IsBuiltIn */


/*
 * RunBuiltInCmd
 *
 * arguments:
 *   commandT *cmd: the command to be run
 *
 * returns: none
 *
 * Runs a built-in command.
 */
	static void
RunBuiltInCmd(commandT* cmd)
{
    char *envpath;
    char *path = malloc(sizeof(char) * 256);
    if(strcmp(cmd->name, "cd")==0)
    {
        if(cmd->argc == 1)
        {
            envpath = getenv("HOME"); //get home if no argument
            strncpy(path, envpath, 256);
        }
        else if(cmd->argv[1][0] == '~')
        {
            envpath = getenv("HOME"); //get home if tilde
            strncpy(path, envpath, 128);
            strncat(path, &cmd->argv[1][1], 128); //concatenate rest of arg on home directory
        }
        else
        {
            strncpy(path, cmd->argv[1], 256);
        }
        if(chdir(path))  //chdir returns non-zero if fail
            Print("Directory could not be found.\n");
    }
    else if (strcmp(cmd->name, "jobs") == 0)
    {
        bgjobL* prev_job = NULL;
        bgjobL* top_job = oldest_bgjob;
        while(top_job != NULL)
        {
            prev_job = top_job;
            top_job = top_job->prev;
            print_job(prev_job, job_status(prev_job));
        }

    }
    else if (strcmp(cmd->name, "fg") == 0)
    {
        fg(cmd);
    }
    else if (strcmp(cmd->name, "bg") == 0)
    {
        bg(cmd);
    }
    free(path);
    freeCommand(cmd);
} /* RunBuiltInCmd */


void
fg(commandT* cmd)
{
    bgjobL *job = NULL;
    if(cmd->argc == 1)
        job = delete_job_num(0);
    else if (cmd->argc == 2)
        job = delete_job_num(atoi(cmd->argv[1]));
    else
        printf("Error: fg takes max one argument\n");
    int Stat;
    fg_pgid = job->pid;
    kill(job->pid, SIGCONT);
    waitpid(job->pid, &Stat, 0);
}

void
bg(commandT* cmd)
{
    bgjobL *job = NULL;
    if(cmd->argc == 1)
        job = continue_job_num(0);
    else if (cmd->argc == 2)
        job = continue_job_num(atoi(cmd->argv[1]));
    else
        printf("Error: bg takes max one argument\n");
    kill(job->pid, SIGCONT);
}

bgjobL*
continue_job_num(int job_num)
{
    if(job_num == 0)
        return bgjobs;
    else
    {
        bgjobL *top_job = bgjobs;
        while(top_job != NULL)
        {
            if(top_job->start_position == job_num)
                return top_job;
            else
                top_job = top_job->next;
        }
    }
    return NULL;
}

bgjobL*
delete_job_num(int job_num)
{
    if(job_num == 0)
        return pop_bg_job(bgjobs->pid);
    else
    {
        bgjobL *top_job = bgjobs;
        while(top_job != NULL)
        {
            if(top_job->start_position == job_num)
                return pop_bg_job(top_job->pid);
            else
                top_job = top_job->next;
        }
    }
    return NULL;
}



/*
 * CheckJobs
 *
 * arguments: none
 *
 * returns: none
 *
 * Checks the status of running jobs.
 */
	void
CheckJobs()
{
   // loop through stack, if a process is done, pop and print
   bgjobL* prev_job = NULL;
   bgjobL* top_job = oldest_bgjob;
   while(top_job != NULL)
   {
       prev_job = top_job;
       top_job = top_job->prev;
       if(job_status(prev_job) == JOB_DONE)
           print_job(prev_job, JOB_DONE);
   }
} /* CheckJobs */

int
job_status(bgjobL* job)
{
    int Stat;
    pid_t wpid;
    wpid = waitpid(job->pid, &Stat, WNOHANG|WUNTRACED);
    if (wpid == 0)
    {
        printf("Running %s\n", job->cmd->name);
        return JOB_RUNNING;
    }
    if (WIFSTOPPED(Stat))
    {
        printf("Stopped %s\n", job->cmd->name);
        return JOB_STOPPED;
    }
    if (WIFEXITED(Stat) || WIFSIGNALED(Stat))
    {
        printf("Done %s\n", job->cmd->name);
        return JOB_DONE;
    }

    return -1;
}

void
print_job(bgjobL* job, const int status)
{
    int is_running = 0;
    char *stat_msg = malloc(sizeof(char) * 20);
    switch(status){
        case 0:
            strcpy(stat_msg, "Running");
            is_running = 1;
            break;
        case 1:
            strcpy(stat_msg, "Stopped");
            break;
        case 2:
            strcpy(stat_msg, "Done");
            break;
    }
    printf("[%d] %s ", job->start_position, stat_msg);
    int i = 0;
    for(i=0; i < job->cmd->argc; i++)
        printf("%s ", job->cmd->argv[i]);
    if(is_running == 1)
        printf(" &");
    printf("\n");
    fflush(stdout);
    if (status == 2)
    {
        pop_bg_job(job->pid);
        free_job(job);
    }
    free(stat_msg);
}


/*
 * FileExists
 *
 * arguments: filename to check
 *
 * returns: int(bool) indicating existence
 *
 * Checks if a file exists by trying to open it
 */

int FileExists(char *fname)
{
	FILE *file;
	if ((file = fopen(fname, "r")))
	{
		fclose(file);
		return 1;
	}
	return 0;
}
