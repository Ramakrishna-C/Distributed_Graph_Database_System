#ifndef UTILS_H
#define UTILS_H

#define MSG_KEY 12345
#define MAX_MSG_SIZE 256

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/shm.h>
#include <signal.h>
#include <pthread.h>
#include  <errno.h>
#include <semaphore.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>

struct graph_data{

    int nodes;
    int adjacencyMatrix[30*30];
    int startVertex;
};

struct payload
{  
    char msg_text[256];
    int sequence_number;
    int operation_number;
    char graphFile_name[256];
    key_t shm_key;
    int server;
};

struct msg_buffer {
    long msg_type;
    struct payload pyld;
};

struct graph_data initializeGraphData (int nodes);
void freeGraph (struct graph_data *graphData);

#endif