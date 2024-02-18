#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

int msgqid;
sem_t *semaphore;
struct graph_data *graphData;
int MAX_NODES = 30;

void *writeRequest(void *a) {
struct payload *pyld = (struct payload *)a;
printf("\npyld->shm_key= %d\n", pyld->shm_key);

if (pyld->shm_key == -1) {
    perror("Invalid shared memory key");
    free(pyld);
    pthread_exit(NULL);
}

int shmid = shmget(pyld->shm_key, sizeof(struct graph_data), 0666 | IPC_CREAT);
if (shmid == -1) {
    perror("shmget");
    free(pyld);
    pthread_exit(NULL);
}

graphData = (struct graph_data *)shmat(shmid, 0, 0);

if (graphData == (void *)-1) {
    perror("shmat");
    printf("shmat errno: %d\n", errno);
    free(pyld);
    pthread_exit(NULL);
}

  FILE *f = fopen(pyld->graphFile_name, "w");

  if (f != NULL) {
    fprintf(f, "%d\n", graphData->nodes);
    for (int i = 0; i < graphData->nodes; i++) {
      for (int j = 0; j < graphData->nodes; j++) {
        fprintf(f, "%d ", graphData->adjacencyMatrix[i * MAX_NODES + j]);
      }
      fprintf(f, "\n");
    }
    fclose(f);

    struct msg_buffer response_msg;
    response_msg.msg_type = pyld->sequence_number;
    response_msg.pyld.server = pyld->server;
    response_msg.pyld.sequence_number = pyld->sequence_number;
    response_msg.pyld.operation_number = pyld->operation_number;
    response_msg.pyld.shm_key = pyld->shm_key;

    if (pyld->operation_number == 1) {
      strcpy(response_msg.pyld.msg_text, "File successfully added\n");
    } else if (pyld->operation_number == 2) {
      strcpy(response_msg.pyld.msg_text, "File successfully modified\n");
    }

    if (msgsnd(msgqid, &response_msg, sizeof(response_msg.pyld), 0) == -1) {
      perror("Error in msgsnd");
    }
  } else {
    perror("Error opening file");
  }

  // Detach shared memory
  if (shmdt(graphData) == -1) {
    perror("shmdt");
  }

  // Free the memory allocated for the payload
  free(pyld);
  pthread_exit(NULL);
}

int main(void) {
  struct msg_buffer msg;

  key_t key = ftok("./loadBalancer.c", MSG_KEY);
  if (key == -1) {
    perror("ftok");
    exit(-1);
  }

  msgqid = msgget(key, 0666);

  if (msgqid == -1) {
    perror("msgget");
    exit(-2);
  }

  printf("Primary Server: Connected to Message Queue with ID %d\n", msgqid);

  semaphore = sem_open("semap", O_CREAT, 0644, 0);
  if (semaphore == SEM_FAILED) {
    perror("Semaphore fail");
    exit(EXIT_FAILURE);
  }

  while (1) {
    if (msgrcv(msgqid, &msg, sizeof(msg.pyld), 1, 0) == -1) {
      perror("msgrcv");
      exit(-3);
    } else {
      pthread_t t;
      struct payload *pyld = malloc(sizeof(struct payload));
      memcpy(pyld, &(msg.pyld), sizeof(struct payload));
      pyld->shm_key = msg.pyld.shm_key;
      strcpy(pyld->graphFile_name, msg.pyld.graphFile_name);

      printf("\n2.file name: %s", msg.pyld.graphFile_name);
      //__sync_add_and_fetch(&pendingRequests, 1);
      if (pthread_create(&t, NULL, writeRequest, (void *)pyld) != 0) {
        perror("pthread_create");
        exit(-4);
      }

      // Wait for the thread to complete
      pthread_join(t, NULL);
      //__sync_sub_and_fetch(&pendingRequests, 1);
    }
  }

  // Detach shared memory (moved outside the loop)
  if (shmdt(graphData) == -1) {
    perror("shmdt");
  }

  sem_close(semaphore);
  sem_unlink("semap");
  return 0;
}
