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
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

typedef struct bgjob_l
{
	pid_t pid;
	struct bgjob_l* next;
} bgjobL;

/* the pids of the background processes */
bgjobL *bgjobs = NULL;

/************Function Prototypes******************************************/
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
Exec(commandT*, bool);
/* runs a builtin command */
static void
RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool
IsBuiltIn(char*);
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
RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
} /* RunCmdPipe */


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
		Exec(cmd, fork);
	else if(strcmp(cmd->name, "exit")!=0) //if exit, let interpreter handle it  
	{
		Print("./tsh line 1: ");
		Print(cmd->name);
		Print(": ");  
		Print("No such file or directory\n");
	}
}  /* RunExternalCmd */


/*
 * ResolveExternalCmd
 *
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
Exec(commandT* cmd, bool forceFork)
{
	pid_t pid;
	char *path;
	char *pathtoken;
	char attemptPath[256];
	bool onPath = FALSE; 

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
		pid = fork(); //fork

		if(pid == 0) //Child
			execv(attemptPath,cmd->argv);
		else //Parent
		{
			wait(NULL); // wait for child to finish
		}
	}
	else
	{
		execv(attemptPath,cmd->argv); //exec without forking
	} 
} /* Exec */


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
	if(strcmp( cmd, "cd")==0) // only built in we need to worry about as of now
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
	free(path);
} /* RunBuiltInCmd */


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
} /* CheckJobs */

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
