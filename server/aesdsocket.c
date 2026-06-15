#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "aesdsocket.h"
//#include <sys/types.h> //system types
#include <sys/stat.h> //system status umask()
#include <stdbool.h> //processing boot as a type
#include <sys/socket.h> // sockets handling - socket(), bind(), accept()
#include <arpa/inet.h> // hton.() fct
#include <syslog.h> // system logging handling syslog()/openlog()/closelog() fnc
#include <unistd.h> // file handling write() fnc
#include <fcntl.h>  // file handling open(() fnc
#include <signal.h> // signals handling

//--------------------------
// definitions section
//--------------------------
//#define DEBUG_MODE_EN                   //to be removed
#define NEW_LINE                   '\n'
#define NULL_TERMINATE             '\0'
//#define IP_ADDRESS          "127.0.0.1"
#define PORT                     (9000) // the port users will be connecting to
#define BACKLOG                     (5) // how many pending connections queue holds
#define PATH_TO_FILE         "/var/tmp"
#define FILE_NAME      "aesdsocketdata"
#define STR_LEN                   (256)
#define BUFFER_SIZE              (1024)
#define IP_ADDRESS_STR_LEN         (16)

#define handle_error(msg)\
   syslog(LOG_ERR, "Error on %s", msg);\
   free(data_buffer);\
   closelog();\
   return EXIT_FAILURE;


//--------------------------
// declarations section
//--------------------------
typedef enum ret_code_t {
   ret_success =  0,
   ret_failed  = -1
} ret_code_type;


/***************************
* Global declarations
****************************/
static volatile bool HANDLING_FLAG = true;


/**
 * @fn check_file
 *     This function checks if a file is existing on the path in the file system
 * @param filedir
 */
/*bool check_file(char* filedir)
{

   return false;
}*/


/**
 * @fn signal_handler
 *  This function handles a signals SIGINT and SIGHALT to interrupt or terminate an application
 * @param signal_number - the signal number
 */
static void signal_handler(int signal_number)
{
   const char *file_name = PATH_TO_FILE "/" FILE_NAME;
   if (   (signal_number == SIGINT)
       || (signal_number == SIGTERM)
       ) {
      syslog(LOG_DEBUG, "Caught signal, exiting");
      HANDLING_FLAG = false;
      remove(file_name);
   }
   exit(EXIT_SUCCESS);
}

/**
 * @fn invoke_daemon
 *     This function provide a service to fork a process and invoke it as deamon
 */
void invoke_daemon()
{
    pid_t pid = fork();

    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // Parent exits

    if (setsid() < 0) exit(EXIT_FAILURE);

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    chdir("/");

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}


/* /@fn write_str_to_file
 *      This function appends a string into a file. If it doesn't exist create this
 * /@param string to be append
 * /@param str_len the number of characters to be written
 * /@param file_path the path to the file in which a string shall be append
 */
ret_code_type write_str_to_file(char* string, int str_len, char* file_path)
{
   ssize_t nr;    //return number of the written symbols into the file

   int ffd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
   if (ffd == -1) {
      /*error*/
      syslog(LOG_ERR, "Error opening file %s", file_path);
      printf("Error opening file %s\n", file_path);
      close (ffd);
      return ret_failed;
   }

   syslog(LOG_DEBUG, "Opening file %s", file_path);

   // Write the text to the file
   nr = write(ffd, string, (size_t)str_len);

   //logging message
#ifdef DEBUG_MODE_EN
   syslog(LOG_DEBUG, "Writing %d characters of \'%s\' to %s", str_len, string, file_path);
   printf("numb of written characters: %d\n", (int)nr);
#endif //DEBUG_MODE_EN
   if ((int)nr == -1 || (int)nr != str_len) {printf("error: nr= %d\n", (int)nr);}

   if (0 != close (ffd)) {
      printf("error close file-descriptor\n");
      syslog(LOG_ERR, "Error closing file %s", file_path);
      return ret_failed;
   }

   syslog(LOG_DEBUG, "Closing file %s", file_path);

   return ret_success;
}

/**
 * @fn send_file_to_socket
 * @param socket_id
 * @param file_path
 */
ret_code_type send_file_to_socket(int socket_id, char* file_path) {
   ssize_t nr_bytes = 0;               // returned number of read data from file
   //char buffer[BUFFER_SIZE];         // to avoid to reserve a large memory area in the stack
   char* buffer = malloc(BUFFER_SIZE); // instead reserve memory in HEAP

   memset(buffer, 0, BUFFER_SIZE);         /* Clear buffer array */

   //open stored file to be sent
   int ffd = open(file_path, O_RDONLY, 0644);
   if (ffd == -1) {
      /*error*/
      syslog(LOG_ERR, "Error opening file %s", file_path);
      printf("Error opening file %s\n", file_path);
      free(buffer); // free the memory
      close(ffd);
      return ret_failed;
   }

   syslog(LOG_DEBUG, "Reading file %s", file_path);

#ifdef DEBUG_MODE_EN
   //read file while NEW_LINE symbol and store to a buffer
   unsigned int bytes = 0;
#endif //DEBUG_MODE_EN

   while ( (nr_bytes = read(ffd, buffer, BUFFER_SIZE)) > 0 ) {
#ifdef DEBUG_MODE_EN
      printf("Read paket data from a file and to be sent to socket: %s", buffer);
      if (buffer[nr_bytes] != NEW_LINE) printf("\n");

      printf("Sending %ld bytes ...\n", nr_bytes);
#endif //DEBUG_MODE_EN
      //send data buffer to a socket
      ssize_t total_sent = 0, byte_sent;
      while (total_sent < nr_bytes) {

         byte_sent = send(socket_id, buffer, nr_bytes, 0);
#ifdef DEBUG_MODE_EN
         printf("%ld bytes have been sent!\n", byte_sent);
#endif // DEBUG_MODE_EN

         if (byte_sent < 0) {
            free(buffer); // free the memory
            close(ffd);
            return ret_failed;
         }
         total_sent += byte_sent;

      } //while

#ifdef DEBUG_MODE_EN
      printf("Sent total bytes: %d\n", (bytes += (int)byte_sent));
#endif //DEBUG_MODE_EN

   } //while

   free(buffer); // free the memory
   close(ffd); //close opened file descriptor
   return ret_success;
}

/**
 * @fn main
 *     The main function
 * @param argc - Number of arguments
 * @param argv - Array of arguments
 */
int main (int argc, char *argv[]) {
   char* data_buffer = malloc(BUFFER_SIZE); //pointer to message data buffer
   //char data_buffer[BUFF_SIZE];
   bool deamon_mode = false;
   char filepath[STR_LEN];
   char ip_str[IP_ADDRESS_STR_LEN];

   //open syslog
   openlog(NULL, 0, LOG_USER); //start syslog

   //register signals
   signal(SIGINT, signal_handler);  //assign SIGINT (e.g. Ctrl-C) to signal-handler
   signal(SIGTERM, signal_handler); //assign SIGTERM (e.g. kill -TERM) to signal-handler

   if (argc == 1) {
#ifdef DEBUG_MODE_EN
      printf("Started in server mode\n");
#endif
      deamon_mode = false;
   }
   else if (argc > 1)
   {
      if (strcmp(argv[1], "-d") == 0)
      {
         deamon_mode = true;
//TODO: undefine definition to avoid use of printf in deamon-mode, because printf won't work (stdout is closed)
#ifdef DEBUG_MODE_EN
         printf("Started in deamon mode\n");
#endif
         invoke_daemon();
      }
      else
      {
         printf("Usage: %s [-d]\n", argv[0]);
         handle_error("arguments");
      }
   }

   memset(data_buffer, 0, BUFFER_SIZE);                 /* Clear array */
   memset(filepath, 0, STR_LEN );                       /* Clear array */
   memset((char*)ip_str, 0, sizeof ip_str);             // clean the string

   //prepare file path to write a message-buffer to a file
   strcat(filepath, PATH_TO_FILE);
   strcat(filepath, "/");
   strcat(filepath, FILE_NAME);
   //filepath [strlen(FILE_PATH)+strlen(FILE_NAME)+2] = NULL_TERMINATE; // Null terminate the string
#ifdef DEBUG_MODE_EN
//      printf("Output file path: %s\n", filepath);
#endif

   //create a socket
   int server_fd = socket(PF_INET, SOCK_STREAM, 0);
   if (server_fd == -1)
   {
      printf("Error on socket creation!\n");
      handle_error("socket");
   }

   // Enable SO_REUSEADDR to avoid bind failing error message 'Address already in use'
   int optval = 1;
   setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

   //Prepare address
   //socket-address instance
   struct sockaddr_in server_sockaddr = {
      .sin_family      = AF_INET,
      .sin_port        = htons(PORT),
      //.sin_addr.s_addr = inet_addr(IP_ADDRESS) //accept only local(host) address
      .sin_addr.s_addr = INADDR_ANY            //accept any address
   };
   memset(server_sockaddr.sin_zero, '\0', sizeof server_sockaddr.sin_zero);

   //now call bind
   int ret_val = bind (server_fd, (struct sockaddr *)&server_sockaddr,
                       sizeof server_sockaddr);
   if (ret_val != 0)
   {
      printf("Error on bind!\n");
      handle_error("bind");
   }
   if (deamon_mode) printf("Started as deamon - shall fork the process. TBD!\n");

   //listening for connection
#ifdef DEBUG_MODE_EN
   printf("Server listening on port %d ...\n", PORT);
#endif
   ret_val = listen(server_fd, BACKLOG);

   // accept connection
   struct sockaddr_in client_addr;
   socklen_t client_addr_len = sizeof client_addr;


   while (HANDLING_FLAG) {//main accept llop - allos to handle more than one connections

      int client_fd = accept(server_fd,
                             (struct sockaddr *)&client_addr,
                             &client_addr_len);
      if (client_fd < 0) perror ("accept failed");

      //ip_str = inet_ntoa(client_addr.sin_addr)
      inet_ntop(AF_INET,
                &client_addr.sin_addr,
                ip_str,
                sizeof(ip_str));

      //write syslog
      syslog(LOG_DEBUG, "Accepted connection from %s", ip_str);
#ifdef DEBUG_MODE_EN
      printf("Client ip address: %s\n", ip_str);
      printf("Client Port: %d\n", ntohs(client_addr.sin_port));
#endif

      // ready to communicate on socket descriptor client_fd!
      char*  pkt_buffer = NULL;
      size_t pkt_size   = 0; //the size of the whole packet
      int    cnt        = 0;
      while (HANDLING_FLAG) {
         //receive packets over the connection
         //it could be separated in more than one packets

         printf("--- Stage Nr.%d ---\n", ++cnt);
         ssize_t bytes_nr = recv(client_fd, data_buffer, BUFFER_SIZE, 0);
         if (bytes_nr <= 0) { printf("Empty packet received -> break!\n"); break; }
         //non-empty packet has been received

#ifdef DEBUG_MODE_EN
         printf("Recived bytes: %d\n", (int)bytes_nr);
#endif
         //expand buffer of the number of recived bytes and allocate memory for tne new cpmplett packet (old + new)
         char* tmp = realloc(pkt_buffer, pkt_size + bytes_nr);
         if (!tmp) {
            syslog(LOG_ERR, "Realloc failed");
            free(tmp);
            break;
         }
         pkt_buffer = tmp; //assign start address of new re-allocated complett packet in memory
         // append data from data-buffer begginning at the end of last recived packet
         memcpy(pkt_buffer + pkt_size, data_buffer, bytes_nr);
         pkt_size += bytes_nr;

      // Process complete packets
      //                                                    ┌──  char says that it is end of complete packet (i - pkt_start_pos + 1)
      //                                                    |
      //                                                    v
      // ┌──────+────────────+──────────────────+─────────────┐
      // │      │            │                  │         '\n'│
      // └──────+────────────+──────────────────+─────────────┘
      // ^                                      ^
      // │                                      │
      // └── start of complette packet          └── start of last packet (pkt_start_pos)
         int pkt_start_pos = 0;
         for (int i = 0; i < pkt_size; i++) {
            if (pkt_buffer[i] == NEW_LINE) {

               //the end of packet has achived
               int packet_len = i - pkt_start_pos + 1; //calculate the length of last received packet

               //append to file the last received packet only
               if (ret_failed == write_str_to_file(pkt_buffer + pkt_start_pos, packet_len, filepath)) {
                  printf("Write file failed.\n"); break;
               }

               // Send full file back over socket-descriptor
               send_file_to_socket(client_fd, filepath);

               pkt_start_pos = i + 1; //current position of new-line character + 1

            } //if (... == NEW_LINE)
         } //for (int i=0; ...

#ifdef DEBUG_MODE_EN
         printf("Packet size: %d\n", (int)pkt_size);
         printf("Packet start position: %d\n", pkt_start_pos);
#endif
         // Remove processed data
         if (pkt_start_pos > 0) {
            //memmove(pkt_buffer, pkt_buffer + pkt_start_pos, pkt_size - pkt_start_pos);
            pkt_size -= pkt_start_pos;
            if (pkt_size == 0) {
               free(pkt_buffer);
               pkt_buffer = NULL;
            } else {
               pkt_buffer = realloc(pkt_buffer, pkt_size);
            }
         } //if

      } //while(1)

      free (pkt_buffer); //free memory

      // close client socket file descriptors
      close(client_fd);
      printf("--- Process completed ---\n");
      printf("Waiting for next connection ...\n\n");
   } // while (1) //handles more than one connections

   // close own server socket file descriptors
   close(server_fd);
   //close socket
   syslog(LOG_DEBUG, "Closed connection from %s", ip_str);
#ifdef DEBUG_MODE_EN
   printf("Sockets closed!\n");
#endif

   //free a message buffer (if created using getaddrinfo-fctn)
   free(data_buffer);

   remove(filepath); //remove stored file

   //close syslog
   closelog();
   return EXIT_SUCCESS;
}

//EOF
