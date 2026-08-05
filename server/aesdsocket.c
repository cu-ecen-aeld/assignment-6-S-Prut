/******************************************************************************
* @file aesdsocket.c
*       This file implements a TCP server
* @author SPrut
* @date   2026-06-19
******************************************************************************/

//--------------------------
// definition section
//--------------------------
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "aesdsocket.h"
//#include <sys/types.h>  // system types
#include <sys/stat.h>   // system status umask()
#include <stdbool.h>    // processing boot as a type
#include <sys/socket.h> // sockets handling - socket(), bind(), accept()
#include <arpa/inet.h>  // hton.() fct
#include <syslog.h>     // system logging handling syslog()/openlog()/closelog() fnc
#include <unistd.h>     // file handling write() fnc
#include <fcntl.h>      // file handling open(() fnc
#include <signal.h>     // signals handling
#include <errno.h>      // error handling

#include <signal.h>
#include <sys/time.h>   // time handling
#include <stdatomic.h>  // use atomic variables to avoid races

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
#define BUFFER_SIZE              (1460)
#define IP_ADDRESS_STR_LEN         (16)
#define TIME_STAMP_STR_LEN         (50)
#define WAIT_TIME_MS            (10000)

#define NC                      "\e[0m"
#define COLOR_RED            "\e[1;31m"
#define COLOR_GREEN          "\e[0;32m"
#define COLOR_YELLOW         "\e[1;33m"
#define COLOR_BLUE           "\e[1;34m"
#define COLOR_MAGENTA        "\e[0;35m"
#define COLOR_CYAN           "\e[0;36m"
#define COLOR_BLACK          "\e[1;30m"
#define COLOR_WHITE          "\e[1;37m"

#define msleep(milliseconds) usleep((unsigned int)milliseconds*1000)

#define handle_error(msg)\
   syslog(LOG_ERR, "Error on %s", msg);\
   closelog();\
   return EXIT_FAILURE;

//--------------------------
// declarations section
//--------------------------
typedef enum ret_code_t {
   ret_success =  0,
   ret_failed  = -1
} ret_code_type;

typedef struct thread_data thread_data_t;


/***************************
* Global declarations
****************************/
volatile sig_atomic_t shutdown_requested = 0;

pthread_mutex_t file_mutex  = PTHREAD_MUTEX_INITIALIZER;
server_data_t   server_data = {0, NULL};


void insertNode(const thread_data_t * const p_node_list, const thread_data_t * const p_new_node)
{
   thread_data_t* p_current_node = (thread_data_t*)p_node_list;
   while (NULL != p_current_node->p_next_node)
   { //look for the end of list
      p_current_node = p_current_node->p_next_node;
   }
   //insert a node at the end of list
   p_current_node->p_next_node = (thread_data_t*)p_new_node;
}

void removeNode(thread_data_t* p_node_list, const thread_data_t* p_node)
{
   thread_data_t* p_current_node = p_node_list;
   thread_data_t* p_previouse_node = NULL;
   while (NULL != p_current_node)
   { //untill not the end of list
      if (p_current_node->thread_id == p_node->thread_id)
      {
         //check if the current node the first node in the list
         if (p_current_node == p_node_list) {
            //remove first node of list

            //is the thread completed
            if (!pthread_join(p_current_node->thread_id, NULL))
            {
               close(p_current_node->client_fd); //close thread descriptor
               p_current_node->thread_finished_success = true;
            }

            p_node_list = p_current_node->p_next_node; //set the list head to the next node
            free(p_current_node); // free the memory

         } else if (NULL == p_current_node->p_next_node) {
            //check if it is last node

            //is the thread completed
            if (!pthread_join(p_current_node->thread_id, NULL))
            {
               close(p_current_node->client_fd); //close thread descriptor
               p_current_node->thread_finished_success = true;
            }

            if (p_previouse_node->p_next_node == p_current_node) {
               p_previouse_node->p_next_node = NULL; //terminate the list
            }
            free(p_current_node); //free memory

         } else {
            //it is not the first and not the last, just remove

            if (!pthread_join(p_current_node->thread_id, NULL))
            {
               close(p_current_node->client_fd); //close thread descriptor
               p_current_node->thread_finished_success = true;

            }

            if (p_previouse_node->p_next_node == p_current_node) {
               p_previouse_node->p_next_node = p_current_node->p_next_node;
            }

            free(p_current_node); //free memory

         }
         return;
      } else
      { //go to the next node in the list
         p_previouse_node = p_current_node;
         p_current_node   = p_current_node->p_next_node;
      }
   }
}

void deleteNodeList(thread_data_t* p_node_list)
{
   thread_data_t* p_current_node = p_node_list;
   while (p_current_node != NULL)
   {

      if (!pthread_join(p_current_node->thread_id, NULL))
      {
         close(p_current_node->client_fd);
         p_current_node->thread_finished_success = true;

      }

      thread_data_t* p_temp_node = p_current_node;
      p_current_node = p_current_node->p_next_node;
      free(p_temp_node); //free memory

   }
   p_node_list = NULL;
}

void setIntervalTimerDescriptorInNodeList(server_data_t* p_server_data, timer_t timerid)
{
   p_server_data->timer_id = timerid;
}

/*****************************************************
* GLOBAL FUNCTIONS
*****************************************************/

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

void shutdown_clients(thread_data_t* p_node_list)
{
   if (p_node_list == NULL) { perror("Clients shutdown failed."); return; }
   thread_data_t* p_current_node = p_node_list;
#ifdef DEBUG_MODE_EN
   uint32_t cnt = 0;
#endif //DEBUG_MODE_EN

   while (p_current_node != NULL)
   {
#ifdef DEBUG_MODE_EN
      printf("Shutting down... clent_tread[%d], #ID %ld\n", ++cnt, p_current_node->thread_id);
#endif //DEBUG_MODE_EN

      //if (!p_current_node->thread_finished_success)
      if (!pthread_join(p_current_node->thread_id, NULL))
      {
         close(p_current_node->client_fd);
         p_current_node->thread_finished_success = true;

      }

      thread_data_t* p_temp_node = p_current_node;
      p_current_node = p_current_node->p_next_node;
      free(p_temp_node);

   }
}


/**
 * @fn signal_handler
 *  This function handles a signals SIGINT and SIGHALT to interrupt or terminate an application
 * @param signal_number - the signal number
 */
void signal_handler(int signal_number)
{
   const char *file_name = PATH_TO_FILE "/" FILE_NAME;

   if (   (signal_number == SIGINT)
       || (signal_number == SIGTERM)
       ) {
      syslog(LOG_DEBUG, "Caught signal, exiting");
      shutdown_requested = 1;
      remove(file_name); //remove temporary file from /var/tmp

      shutdown_clients(server_data.p_thread_node_list);

      printf("Interval-Timer stoped!\n");
      timer_delete(server_data.timer_id);

      //destroy file_mutex & close server socket
      printf("Closing server socket...\n");
      close(server_data.server_fd);
      printf("Destroing mutex...\n");
      pthread_mutex_destroy(&file_mutex);
   }

   //close syslog
   printf("Closing logging...\n");
   closelog();

   printf("Server finished! Exit.");

   exit(EXIT_SUCCESS);
}


/* /@fn write_packet
 *      This function appends a packet data into a file. If it doesn't exist create this
 * /@param data to be append
 * /@param data_len the number of characters to be written
 * /@param file_path the path to the file in which a string shall be append
 */
ret_code_type write_packet(char* data, int data_len, const char* file_path)
{
   //critical section
   pthread_mutex_lock(&file_mutex);

   int ffd = open(file_path, O_WRONLY | O_APPEND | O_CREAT, 0644);
   if (ffd == -1) {
      /*error*/
      syslog(LOG_ERR, "Error opening file %s", file_path);
      printf("Error opening file %s\n", file_path);
      close (ffd);
      return ret_failed;
   }

   syslog(LOG_DEBUG, "Opening file %s", file_path);

   // Write data buffer to the file
   ssize_t nr = write(ffd, data, (size_t)data_len); //it returns number of the written symbols into the file

   //logging message
#ifdef DEBUG_MODE_EN
   syslog(LOG_DEBUG, "Writing %d characters of \'%s\' to %s", data_len, data, file_path);
   printf("numb of written characters: %d\n", (int)nr);
#endif //DEBUG_MODE_EN
   if ((int)nr == -1 || (int)nr != data_len) {printf("error: nr= %d\n", (int)nr);}

/*   if (0 != close (ffd)) {
      perror("error close file-descriptor");
      syslog(LOG_ERR, "Error closing file %s", file_path);
      return ret_failed;
   }*/
   close (ffd);
   pthread_mutex_unlock(&file_mutex);
   //end of critical section

   syslog(LOG_DEBUG, "Closing file %s", file_path);
   return ret_success;
}


/**
 * @fn send_file_to_socket
 * @param socket_id - socket file descriptor
 * @param file_path - path and name of file to be read from
 * @return ret_code_type - the result of sending
 */
ret_code_type send_file_to_socket(int socket_id, const char* file_path) {
   ssize_t nr_bytes = 0;                 // returned number of read data from file
   //char* buffer = malloc(BUFFER_SIZE);   // reserve memory in HEAP, donot use stack
   //memset(buffer, 0, BUFFER_SIZE);       // Clear buffer array, NULL-terminators
   char* buffer = (char*)calloc(BUFFER_SIZE, sizeof(char));   // eserve memory in HEAP with initializing, donot use stack

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

   //read file while NEW_LINE symbol and store to a buffer
#ifdef DEBUG_MODE_EN
   unsigned int total_sent = 0;
   printf("#Start packet %s[%s", COLOR_YELLOW, NC);
#endif //DEBUG_MODE_EN

   //BUFFER_SIZE-1 subtract 1 for NULL-terminater at the end
   while ( (nr_bytes = read(ffd, buffer, BUFFER_SIZE-1)) > 0 ) {
#ifdef DEBUG_MODE_EN
      printf("\nRead paket data from a file and sent to socket...\n");
      printf("Sending %ld bytes ...\n", nr_bytes);
      printf("%s%s%s", COLOR_GREEN, buffer, NC);
      if (buffer[nr_bytes-1] != NEW_LINE) printf("%s...%s", COLOR_GREEN, NC);
      else printf("%s]%s #End packet", COLOR_YELLOW, NC);

#endif //DEBUG_MODE_EN
      //send data buffer to a socket
      ssize_t bytes = 0, byte_sent;
      while (bytes < nr_bytes) {

         byte_sent = send(socket_id, buffer, nr_bytes, 0);
#ifdef DEBUG_MODE_EN
         printf("\n%ld bytes have been sent!\n", byte_sent);
#endif // DEBUG_MODE_EN

         if (byte_sent < 0) {
            free(buffer); // free the memory before return
            close(ffd);
            return ret_failed;
         }
         bytes += byte_sent;

      } //while

#ifdef DEBUG_MODE_EN
      printf("\n%sSent total bytes: [%d]%s\n", COLOR_MAGENTA, total_sent += (int)bytes, NC);
#endif //DEBUG_MODE_EN

   } //while

   free(buffer); // free the memory before return
   close(ffd);   //close opened file descriptor

   syslog(LOG_DEBUG, "Closing file %s", file_path);

   return ret_success;
}


bool print_clock_time( clockid_t id, const char *clocktype, struct timespec *ts )
{
    bool success = false;
    int rc = clock_gettime(id,ts);
    if( rc != 0 ) {
        printf("Error %d (%s) getting clock %d (%s) time\n",
                errno,strerror(errno),id,clocktype);
    } else {
        printf("Clock %d (%s) %ld.%09ld\n",id,clocktype,ts->tv_sec,ts->tv_nsec);
        success = true;
    }
    return success;
}



/**
* @fn client_thread
*     client thread handler shall perform the receiving data over few packages from client,
*     unpack them and put into file as terminated (\n) message, Additionally it should
*     send the received package back to the client as acknowledge
* @param p_thread_data - list of function parameters
*/
void* client_thread(void* p_thread_data)
{
   thread_data_t* thread_data_p = 0;
   if (p_thread_data == NULL) { perror("Thread data failed"); return NULL; } //break thread
   thread_data_p = (thread_data_t*)p_thread_data;

   char* data_buffer = malloc(BUFFER_SIZE); //allocate data buffer in heap
   memset(data_buffer, 0, BUFFER_SIZE); //clear data buffer, NULL-terminated buffer

   //ready to communicate on socket descriptor connection
   char*  pkg_buffer = NULL; // msg buffer
   size_t pkg_length = 0;
   const char *file_name = PATH_TO_FILE "/" FILE_NAME "\0";
#ifdef DEBUG_MODE_EN
   int cnt = 0;
#endif // DEBUG_MODE_EN

   //receive loop - receive all packages
   //thread exits when shutdown requested
   while (!shutdown_requested)
   {
#ifdef DEBUG_MODE_EN
      printf("--- Stage Nr.%d ---\n", ++cnt);
#endif // DEBUG_MODE_EN

      ssize_t bytes_received = recv(thread_data_p->client_fd,
                                    (void*)data_buffer,
                                    BUFFER_SIZE-1, //subtract 1 for the NULL-terminator at the end
                                    0);


#ifdef DEBUG_MODE_EN
      printf("Recived bytes: %d\n", (int)bytes_received);
#endif
      //Thread exit when recv() <= 0 - client disconnected(0) or error(<0)
      if (bytes_received <= 0) { printf("Empty packet received -> break!\n"); break; }
      //non-empty packet has been received

      //dynamic packet buffer
      char* tmp_buff = realloc(pkg_buffer, pkg_length + bytes_received);
      if (tmp_buff == NULL) {
         syslog(LOG_ERR, "Realloc failed");
         free(tmp_buff);
         break; //Unable to resize memory
      }

      pkg_buffer = tmp_buff;//assign start address of new re-allocated complett packet in memory
      memset(pkg_buffer+pkg_length, 0, bytes_received); //clear re-allocated buffer space
      // append data from data_buffer begginning at the end of last recived packet
      memcpy(pkg_buffer+pkg_length, data_buffer, bytes_received);
      pkg_length += bytes_received;

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
      //detect new-line symbol
      int pkg_start_pos = 0;
      for (int i = 0; i < pkg_length; i++) {
         if (pkg_buffer[i] == NEW_LINE) {

            //the end of packet has achived
            int packet_len = i - pkg_start_pos + 1; //calculate the length of last received packet
#ifdef DEBUG_MODE_EN
            printf("write to %s\n", file_name);
#endif //DEBUG_MODE_EN
            //append to file the last received packet only

            //if (write_pkg_to_file(pkg_buffer + pkg_start_pos, file_name) < 0) { printf("Write file failed.\n"); break;}
            if (ret_failed == write_packet(pkg_buffer + pkg_start_pos, packet_len, file_name)) {
               printf("Write file failed.\n"); break;
            }

            // Send full file back over socket-descriptor
            if (ret_failed == send_file_to_socket(thread_data_p->client_fd, file_name)) {
               printf("Send data back failed! "); break;
            }

            pkg_start_pos = i + 1; //current position of new-line character + 1

         } //if (... == NEW_LINE)
      } //for (int i=0; ...

   } //while (!shutdown_requested)

   close(thread_data_p->client_fd);
   free(data_buffer);
   free(pkg_buffer);

   thread_data_p->thread_finished_success = true;

   printf("Client thread completed!\n");
   return NULL;
}


/* SIGEV_SIGNAL solution
atomic_int timerExpired = 0;
void timer_handler(int signo)
{
   timerExpired = 1;
}

void interval_timer_callback (int sig)
 End of SIGEV_SIGNAL solution */
/**
* @fn interval_timer_callback
*     interval timer callback handler shall perform the timer signal interrupting
*     and store a timestamp into a file
* @param sv - union sigval
*/
void interval_timer_callback (union sigval sv)
{
   //const char *file = PATH_TO_FILE "/timestamps.txt\0";
   const char *file = PATH_TO_FILE "/" FILE_NAME "\0";
   if (!shutdown_requested) {
      struct timespec ts_realtime_start;
      clockid_t id = CLOCK_REALTIME;

      int rc = clock_gettime(id, &ts_realtime_start);
      if( rc != 0 ) {
         printf("Error %d (%s) getting clock %d (%s) time\n",
                 errno,strerror(errno),id,"CLOCK_REALTIME");
      } else {
         //msleep(WAIT_TIME_MS);
         struct tm tm;
         if ( gmtime_r(&ts_realtime_start.tv_sec,&tm) == NULL ) {
            printf("Error calling gmtime_r with time %ld\n",ts_realtime_start.tv_sec);
         } else  {

            char timestamp[TIME_STAMP_STR_LEN] = {0}, result[TIME_STAMP_STR_LEN] = {0};
            //char* timestamp = (char*)calloc(TIME_STAMP_STR_LEN, sizeof(char));
            //char* result    = (char*)calloc(TIME_STAMP_STR_LEN, sizeof(char));
            if( strftime(result,sizeof(result),"%a, %d %b %Y %T %z",&tm) == 0 ) {
               printf("Error converting string with strftime\n");
            } else {

               if (TIME_STAMP_STR_LEN > (int)strlen(result)) {
                  strcat(timestamp, "timestamp:");
                  strcat(timestamp, (const char*)result);
                  strcat(timestamp, "\n");
               }
               syslog(LOG_DEBUG, "Writing time-stamp");
#ifdef DEBUG_MODE_EN
               printf("%sWriting file %s ...%s\n", COLOR_YELLOW, file, NC);
#endif //DEBUG_MODE_EN
               if (ret_failed == write_packet(timestamp, strlen(timestamp), file)) {
                  printf("Write file failed.\n"); return;
               }

            }//if( strftime(..))
            //free(result);
            //free(timestamp);
         } //if ( gmtime_r(...))
      } //if (rc!=0)
   }

} //time_handler

void setup_interval_timer()
{
   //start UNIX solution (depricated)
/*   struct sigaction sa;
   struct itimerval timer;
   sa.sa_handler = interval_timer_callback;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = 0;
   sigaction(SIGALRM, &sa, NULL);

   timer.it_value.tv_sec     = 3;      // first alarm after 10 s
   timer.it_value.tv_usec    = 0;

   timer.it_interval.tv_sec  = 3;   // than all 10 s
   timer.it_interval.tv_usec = 0;

   //arming the timer
   if (0 != setitimer(ITIMER_REAL, &timer, NULL)) { perror("setitimer"); };
*/   // end UNIX solution

   //start POSIX-based solution
   struct itimerspec      ts;
   struct sigevent       evp;
   timer_t           timerid;

   //SIGEV_SIGNAL solution
/*   struct sigaction       sa;
   sa.sa_handler = timer_handler;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags   = 0;
   sigaction(SIGUSR1, &sa, NULL);

   evp.sigev_value.sival_ptr = &timerid;
   evp.sigev_notify          = SIGEV_SIGNAL;
   evp.sigev_signo           = SIGUSR1;
*/

   //alternative SIGEV_THREAD solution
   evp.sigev_value.sival_ptr   = NULL;
   evp.sigev_notify            = SIGEV_THREAD;
   evp.sigev_notify_attributes = NULL;
   evp.sigev_notify_function   = interval_timer_callback;

   //create a timer
   if (0 != timer_create (CLOCK_REALTIME,
                          &evp,
                          &timerid))
   { perror ("timer_create"); }

   //configure periodic timer that expires every second
   ts.it_interval.tv_sec  = 10; //first time timer triggers
   ts.it_interval.tv_nsec = 0;
   ts.it_value.tv_sec     = 10; //how offen timer triggers further
   ts.it_value.tv_nsec    = 0;

   //arming the timer
   if (0 != timer_settime (timerid,
                           0,
                           &ts,
                           NULL))
   { perror ("timer_settime"); }
   server_data.timer_id = timerid;
   //end POSIX-based solution (SIGEV_SIGNAL/SIGEV_THREAD

   //start POSIX-based timer thread solution
   //end POSIX-based timer thread solution
   printf("Interval-Timer started...\n");

   //timer_delete(&timerid);
   //printf("Interval-Timer stoped!\n");

} //setup_interval_timer


void setup_server()
{
   //Setup server
   //server_fd=socket (...) --> bind (server_fd, ...) --> listen(server_fd, ...)

   char ip_str[IP_ADDRESS_STR_LEN] = {0};
   //memset((char*)ip_str, 0, sizeof(ip_str));             // clean the string
   //Server setup - create a socket
   int server_descriptor = socket(PF_INET, SOCK_STREAM, 0);
   if (server_descriptor < 0)
   {
      printf("Error on socket creation!\n");
   }

   // Enable SO_REUSEADDR to avoid bind failing error message 'Address already in use'
   int optval = 1;
   setsockopt(server_descriptor,
              SOL_SOCKET,
              SO_REUSEADDR,
              &optval,
              sizeof(optval));

   //Prepare address
   //server socket-address instance
   struct sockaddr_in server_sockaddr = {
      .sin_family      = AF_INET,
      .sin_port        = htons(PORT),
      //.sin_addr.s_addr = inet_addr(IP_ADDRESS) //accept only local(host) address
      .sin_addr.s_addr = INADDR_ANY            //accept any address
   };
   memset(server_sockaddr.sin_zero, '\0', sizeof(server_sockaddr.sin_zero));

   //now call bind
   int ret_val = bind (server_descriptor,
                       (struct sockaddr *)&server_sockaddr,
                       sizeof(server_sockaddr));
   if (ret_val != 0)
   {
      printf("Error on bind!\n");
   }

   //----------------------------
   // listening for connection
   //----------------------------
   printf("Server listening on port %d ...\n", PORT);
   ret_val = listen(server_descriptor, BACKLOG);
   if (ret_val != 0)
   {
      printf("Error on port listen!\n");
   }

   //----------------------------
   // accept clent connection
   //----------------------------
   struct sockaddr_in client_addr;
   socklen_t client_addr_len = sizeof client_addr;

   while (!shutdown_requested)
   {
      //SIGEV_SIGNAL solution
/*      if (timerExpired)
      {
         timerExpired = 0;
         interval_timer_callback(SIGUSR1);
      }
*/

      //receive packets over the connection, endless loop
      //it coud be separated into more than one packet
      int client_descriptor = accept(server_descriptor,
                                     (struct sockaddr *)&client_addr,
                                     &client_addr_len);
      if (client_descriptor < 0)
      {
         continue;
      }

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

      //spawn a new thread
      pthread_t thread_id;
      //allocate node
      thread_data_t* list_p = (thread_data_t*)malloc(sizeof(thread_data_t));
      thread_data_t* p_last_node;
      list_p->client_fd = client_descriptor;
      list_p->thread_finished_success = false;
      list_p->p_next_node = NULL;

      //thread-node management
      if (server_data.p_thread_node_list == NULL) {
         //init the list via first entry
         server_data.p_thread_node_list = list_p;
         server_data.server_fd = server_descriptor;
      }
      else {
         //look for last entry in the list
         p_last_node = server_data.p_thread_node_list;
         while (p_last_node->p_next_node != NULL)
         {
            p_last_node = p_last_node->p_next_node;
         }
         //insert new node into the list
         p_last_node->p_next_node = list_p;
      }

      //create thread
      int rc = pthread_create(&thread_id,
                              NULL,
                              client_thread,
                              (void*)list_p);
      if (rc == 0) {
         //Thread successfully started
         printf("%sCreated thread-ID: %ld%s\n", COLOR_GREEN, thread_id, NC);
         //insert thread-node into list
         list_p->thread_id = thread_id;
      }
      //else thread creation failed

   } //while(!shutdown_requested)
} //setup_server

/**
 * @fn main
 *     The main function
 * @param argc - Number of arguments
 * @param argv - Array of arguments
 */
int main (int argc, char *argv[]) {

   bool deamon_mode = false;

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

   //setting up and arming a timer
   setup_interval_timer();

   if (deamon_mode) printf("Started as deamon - shall fork the process. TBD!\n");
   setup_server();

   //close syslog
   closelog();
   return EXIT_SUCCESS;
}

//EOF
