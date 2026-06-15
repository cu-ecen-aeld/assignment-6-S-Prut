#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>

//--------------------------
// definitions section
//--------------------------
//#define DEBUG_MODE_EN
//#define USE_STDIO

#ifndef USE_STDIO
  #include "fcntl.h"
#endif

#define STRING_LENGTH 256
#define MKDIR(path) mkdir(path, 0755)

//--------------------------
// declarations section
//--------------------------
typedef enum ret_code_t {
   ret_success=0,
   ret_failed=1
} ret_code_type;

/*int ensure_file_direcory_exists(char* filepath)
{
   //int retVal = MKDIR(filepath);
   //if (retVal != 0) {
   //   //mkdir returns -1 and sets errno if directory exists of rails
   //   printf("Warning: could not create directory or it already exists:\n");
   //}
   struct stat st;
   if (stat(filepath, &st) == 0) {
      //in case fo a directory path - path exists
      //return S_ISDIR(st.st_mode);
      //file exists
      return S_ISREG(st.st_mode);
   }
   // stat(failed) - path does not exist
   printf("Warning: required directory or file is not exist!\n");
   return 0;
}*/

ret_code_type write_str_to_file(char* searchstring, char* filedir)
{
//first variant: use STDIO library
#ifdef USE_STDIO
   FILE *fdesc_p; //file-dascriptor to be opened
   fdesc_p = fopen (filedir, "a+");
   if (fdesc_p == NULL) {
      syslog(LOG_ERR, "Error opening file %s", filedir);
      printf("Error opening file %s\n", filedir);
      return ret_failed;
   }
   syslog(LOG_DEBUG, "Opening file %s", filedir);
   fprintf(fdesc_p, "%s: %s\n",
           "added automatically", searchstring);
#ifdef DEBUG_MODE_EN
   printf("Writing string \'%s\' to the file %s\n", searchstring, filedir);
#endif //DEBUG_MODE_EN
   syslog(LOG_DEBUG, "Writing \'%s\' to %s", searchstring, filedir);
   fclose(fdesc_p);
   syslog(LOG_DEBUG, "Closing file %s", filedir);
#else //second variant use syscall
   int close_ret; //return value after closing the opened file
   ssize_t nr; //return number of the written symbols into the file
   int fd = open(filedir, O_WRONLY | O_APPEND | O_CREAT, 0644);
   if (fd == -1) {
      /*error*/
      syslog(LOG_ERR, "Error opening file %s", filedir);
      printf("Error opening file %s\n", filedir);
      return ret_failed;
   }
   syslog(LOG_DEBUG, "Opening file %s", filedir);
   // Write the text to the file
   nr = write(fd, searchstring, strlen(searchstring));
   write(fd, "\n", 1); //insert new line character into file
   syslog(LOG_DEBUG, "Writing \'%s\' to %s", searchstring, filedir);
#ifdef DEBUG_MODE_EN
   printf("numb of written symbols: %d\n", (int)nr);
#endif //DEBUG_MODE_EN
   if (nr == -1 || nr != strlen(searchstring)) {printf("error: nr= %d\n", (int)nr);}
   close_ret = close (fd);
   syslog(LOG_DEBUG, "Closing file %s", filedir);
#endif //USE_STDIO

   return ret_success;
}

int main (int argc, char *argv[])
{
   char filedir[STRING_LENGTH], searchstr[STRING_LENGTH], filePath[STRING_LENGTH];
   openlog(NULL, 0, LOG_USER); //start syslog

   //-----------------------------------------------------------------------------
   printf("You have entered %d arguments\n", argc-1);

   if (argc == 1) {
      printf("No extra command line argument passed!\n");
      syslog(LOG_ERR, "Invalid number of arguments: %d", argc);
      closelog();
      return EXIT_FAILURE;
   }

   if (argc >= 2) {
      for (int i=1; i<argc; i++) {
         printf("argument %d: %s\n", i, argv[i]);
         switch (i)
         {
            case 1:
               strcpy(filedir, argv[i]);
               break;
            case 2:
               strcpy(searchstr, argv[i]);
               break;
            default:
               printf("unexpected number of arguments: %d\n", i);
               syslog(LOG_ERR, "Unexpected number of arguments: %d", argc);
         }
      }
      char* fileName = strrchr(filedir, '/');
      if (fileName != NULL){
         int dirLength = fileName - filedir;
         fileName = fileName+1;
#ifdef DEBUG_MODE_EN
         printf("dir length: %d\n", dirLength);
#endif //DEBUG_MODE_EN
         strncpy(filePath, filedir, dirLength);
         filePath[dirLength] = '\0'; //Null-terminate the string
      }
      else
      {
         strcpy(filePath, ".");
         fileName = filedir;
      }
#ifdef DEBUG_MODE_EN
      printf("dir path: %s\n", filePath);
      printf("file name: %s\n", fileName);
      ensure_file_direcory_exists(filedir);
#endif //DEBUG_MODE_EN

      if (write_str_to_file(searchstr, filedir) == ret_failed){closelog(); return EXIT_FAILURE;}
   } //if (argc >= 2)

   closelog();
   return EXIT_SUCCESS;
}
