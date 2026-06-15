#include "systemcalls.h"
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include "fcntl.h"
#include <sys/wait.h>
#include <syslog.h>

int checkifexecutable(const char *cmdname)
{
    int result;
    struct stat statinfo;

    result = stat(cmdname, &statinfo);
    if (result < 0) return 0;
    if (!S_ISREG(statinfo.st_mode)) return 0;

    if (statinfo.st_uid == geteuid()) return statinfo.st_mode & S_IXUSR;
    if (statinfo.st_gid == getegid()) return statinfo.st_mode & S_IXGRP;
    return statinfo.st_mode & S_IXOTH;
}

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret = system (cmd);
    if (ret == -1) {
        perror ("system");
        return false;
    }
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    for(i=0; i<count; i++)
    {
        if (i == 0) printf("**********************\nCommand: %s\n", command[i]);
        else printf("Argument(s)[%d]: %s\n", i, command[i]);
    }
    printf("Argument(s)[%d]: %s\n", i, command[i]);
    int   ret;
    pid_t kidpid;

    if (access(command[count-1], F_OK) != 0) {
        //fprintf(stderr, "error: command %s doesnot exist.\n", command[count-1]);
        syslog(LOG_ERR, "error: command %s doesnot exist.\n", command[count-1]);
        return false;
    }
    fflush(stdout); //to avoid double output in printf after fork() call
    kidpid = fork();
    if (kidpid == -1) {
        perror ("fork");
        return false;
    }
    if (kidpid == 0) {
        /*child process has been invoked*/
        printf("My PID is %d\n", kidpid);
        /*executing another pprogramm/command in the child process*/
        ret = execv(command[0], command);
        //if execv has returned - there is an error
        if (ret == -1) {
            perror ("execv");
            exit(EXIT_FAILURE); //return false;
        }
        return false;
    } else {
        // the parent process - wait utill child has terminated
        int status;
        // wait until the child process ends
        if (waitpid(kidpid, &status, 0) == -1) {
            perror("waitpid");
            return false;
        }
        //check if child was completed successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else
            return false;
    }

    va_end(args);

    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    for(i=0; i<count; i++)
    {
        if (i == 0) printf("**********************\nCommand: %s\n", command[i]);
        else printf("Argument(s)[%d]: %s\n", i, command[i]);
    }
    printf("Argument(s)[%d]: %s\n", i, command[i]);
    pid_t pid;
    fflush(stdout); //to avoid double output in printf after fork() call
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644); //open a file in which it redirects
    if (fd < 0) { perror("open"); return false; }
    switch (pid = fork()) {
      case -1: close(fd); perror("fork"); return false;
      case 0:
          //child process
          if (dup2(fd, 1) < 0) { perror("dup2"); return false;}
          close(fd);
          execv(command[0], (char* const*)command);
          //if execv has returned - there is an error
          perror ("execv");
          exit(EXIT_FAILURE); //return false;
      default:
          close(fd);
          /* do whatever the parent wants to do. */
          // the parent process - wait utill child has terminated
          int status;
          // wait until the child process ends
          if (waitpid(pid, &status, 0) == -1) {
              perror("waitpid");
              return false;
          }
          //check if child was completed successfully
          if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
              return true;
          else
              return false;
    }

/*    fflush(stdout); //to avoid double output in printf after fork() call
    pid = fork();
    if (pid == -1) {
        perror ("fork");
        return false;
    }
    if (pid == 0) {
        //child process
        printf("My PID is %d\n", pid);
        int ret = execv(command[0], (char* const*)command);
        if (ret == -1) {
            perror ("execv");
            exit(EXIT_FAILURE); //return false;
        }
    } else {
        // the parent process - wait utill child has terminated
        int status;
        // wait until the child process ends
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return false;
        }
        //check if child was completed successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            return true;
        else
            return false;
    }*/

    va_end(args);

    return true;
}
