/***************************************************************************
 *  Title: tsh
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.4 $
 *    Last Modification: $Date: 2009/10/12 20:50:12 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80

/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
static void
sig(int);

void
InitializeShell();

void
TranslatePrompt(char*, char*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

/*
 * main
 *
 * arguments:
 *   int argc: the number of arguments provided on the command line
 *   char *argv[]: array of strings provided on the command line
 *
 * returns: int: 0 = OK, else error
 *
 * This sets up signal handling and implements the main loop of tsh.
 */
int
main(int argc, char *argv[])
{
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*) * BUFSIZE);
  char* prompt;
  char* translatedPrompt = malloc(sizeof(char) * BUFSIZE);
 /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR)
    PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR)
    PrintPError("SIGTSTP");

  InitializeShell();
 
  while (!forceExit) /* repeat forever */
    {

 
      prompt = getenv("PS1"); //Check for a set PS1 variable
      if(prompt != NULL)
	{
		TranslatePrompt(prompt, translatedPrompt); //If found, translate it and use it
		Print(translatedPrompt);
	}
      /* read command line */
      getCommandLine(&cmdLine, BUFSIZE);

      /* checks the status of background jobs */
      CheckJobs();

      /* interpret command and line
       * includes executing of commands */
      Interpret(cmdLine);

      if (strcmp(cmdLine, "exit") == 0)
        forceExit = TRUE;
    }

  /* shell termination */
  free(cmdLine);
  free(translatedPrompt);
  return 0;
} /* main */

/*
 * sig
 *
 * arguments:
 *   int signo: the signal being sent
 *
 * returns: none
 *
 * This should handle signals sent to tsh.
 */
static void
sig(int signo)
{
//	if(signo == SIGINT)
//		exit(0);
} /* sig */

/*
 * InitializeShell
 *
 * arguments: none
 *
 * returns: none
 *
 * This initializes the shell, reading in the .*rc file
 */
void
InitializeShell()
{
	FILE *file;
	char *home = getenv("HOME"); //Get home directory
	size_t const len = strlen(home)+1;
	char *configFile = memcpy(malloc(len+7), home, len); //copy it into a char * with room for filename
	char *readBuffer = NULL;
	size_t rlen = 0;
	ssize_t read;
	strncat(configFile,"/.tshrc",7);	//add filename on to home directory
	if((file = fopen(configFile,"r")))
	{	//printf("Opened config: %s\n", configFile);
		while( (read = getline(&readBuffer, &rlen, file)) != -1) //loop through lines
		{
			//remove new lines, null terminate
			if(readBuffer[read-1] == '\n')
				readBuffer[read-1] = 0;
			Interpret(readBuffer);
		} 
		fclose(file);
	}
	
	free(configFile);
	free(readBuffer); 
}

/*
 * TranslatePrompt
 *
 * arguments: pointer to the buffer with prompt and the buffer to write to
 *
 * returns:none
 *
 * This translates the PS1 environment variable to lookup information
 */
void
TranslatePrompt(char *prompt, char *tprompt)
{
 memset(tprompt, 0, sizeof(char) * BUFSIZE); //Clear out translated prompt
 char *hostname = malloc(sizeof(char) * BUFSIZE);
 char timestring[BUFSIZE];
 char workingdir[BUFSIZE];
 char *username;
 username = getenv("USER"); //get username
 size_t ulength = strlen(username); 
 gethostname(hostname,BUFSIZE); //put hostname in hostname pointer
 hostname = strtok(hostname,"."); //cut off at first dot
 size_t hlength = strlen(hostname); 
 getcwd(workingdir, BUFSIZE); //get working directory
 size_t wlength = strlen(workingdir);
 time_t curtime = time(NULL); //get time
 struct tm *tmp = localtime(&curtime); //put it as local time
 strftime(timestring, BUFSIZE,"%T", tmp); //format time
 size_t tlength = strlen(timestring);
 
 int strlen = 0;
 int newstrlen = 0;
 while(prompt[strlen] != 0)
 {
 	if(prompt[strlen] == '\\') //if backslash is found, look at next character
		switch(prompt[strlen+1])
		{	
		  case 'u':
			strncat(tprompt, username, ulength); //concatenate username to new string
			strlen+=2; //increment past both backslash and u
			newstrlen += ulength; //incrememnet new string for length of username
			break;
		  case 'h':
			strncat(tprompt, hostname, hlength);
			strlen+=2;
			newstrlen += hlength;
			break;
		  case 'w':
			strncat(tprompt, workingdir, wlength);
			strlen+=2;
			newstrlen += wlength;
			break;
		  case 't':
			strncat(tprompt, timestring, tlength);
			strlen+=2;
			newstrlen += tlength;
			break;
		  default:
			tprompt[newstrlen] = prompt[strlen];
			strlen++;
			newstrlen++;
			break;
 		}
	else //must not have be special character
	{
		tprompt[newstrlen] = prompt[strlen]; //put old char in new string 
		strlen++; //increment both counters
		newstrlen++;
	}	
 
 }
free(hostname);

}
