#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdbool.h> //bool data type declaration and handling
#include <pthread.h> //POSIX thread handling

struct thread_data {
    //pthread_mutex_t* mutex_p; //to be deleted if not required
    int              client_fd; // client descriptor
    pthread_t        thread_id; //thread descriptor

    /**
     * Set to true if the thread completed with success, false
     * if an error occurred.
     */
    bool             thread_finished_success;

    struct thread_data* p_next_node;
};

typedef struct SinglyLinkedList {
    struct thread_data *head;
    struct thread_data *tail;
    int                 size;
} SinglyLinkedList;

typedef struct server_data {
   int                 server_fd;
   timer_t             timer_id;
   struct thread_data* p_thread_node_list;
} server_data_t;
#endif //AESDSOCKET_H
