#include "utils.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>

#define MAX_NODES 30

int msgqid;
sem_t *semaphore;
struct graph_data *graphData;
struct DFSUtilArgs dfsArgs;
int nodes;
int adjMat[MAX_NODES][MAX_NODES];
pthread_mutex_t mutex;
pthread_mutex_t r_mutex;
int serverNumber;
void *DFSUtilThread(void *args);

void *BFSUtilThread(void *args);
void *ProcessNodeThread(void *args);
struct BFSUtilArgs {
  struct graph_data *graphData;
  int *visited;
  struct TraversalResult *result;
  int *current_level;
  int *nc;
  int *next_level;
  int *nn;
};

struct ProcessNodeArgs {
  int startVertex;
  int *visited;
  int *next_level;
  int *nn;
};
void *DFSUtilThread(void *args);
struct DFSUtilArgs {
  int vertex;
  struct graph_data *graphData;
  int *visited;
  struct TraversalResult *result;
  int *deepestNode;
};
struct TraversalResult {
  int *vertices;
  int count;
};

// Utility function for DFS
void DFSUtil(int vertex, struct graph_data *graphData, int *visited,
             struct TraversalResult *result) {
  printf("\nRunning DFS for vertex: %d, count: %d\n", vertex, result->count);
  // Acquire the mutex before updating the shared array
  pthread_mutex_lock(&mutex);
  // Mark the current node as visited
  visited[vertex] = 1;
  // Release the mutex
  pthread_mutex_unlock(&mutex);

  pthread_t threads[MAX_NODES];
  int n_threads = 0;
  // Create a new thread for every unvisited neighbor
  for (int i = 0; i < nodes; i++) {
    pthread_mutex_lock(&r_mutex);
    int seen = visited[i];
    pthread_mutex_unlock(&r_mutex);
    if (adjMat[vertex][i] == 1 && !seen) {
      printf("\nCreating thread for vertex %d\n", i);
      struct DFSUtilArgs *dfsArg =
          (struct DFSUtilArgs *)malloc(sizeof(struct DFSUtilArgs));
      dfsArg->vertex = i;
      dfsArg->graphData = graphData;
      dfsArg->visited = visited; // Assuming 'visited' is a global array
      dfsArg->result = result;
      if (pthread_create(&threads[n_threads++], NULL, DFSUtilThread,
                         (void *)dfsArg) != 0) {
        perror("pthread_create");
        exit(-4);
      }
    }
  }
  // Wait for all threads to finish
  for (int t = 0; t < n_threads; t++) {
    pthread_join(threads[t], NULL);
  }

  pthread_mutex_lock(&mutex);
  if (n_threads == 0) {
    result->vertices[result->count++] = vertex;
  }
  pthread_mutex_unlock(&mutex);
  printf("\nvertex: %d, Threads are done!\n", vertex);
}

// DFSUtil function that accepts a void pointer as its argument
void *DFSUtilThread(void *args) {
  struct DFSUtilArgs *dfsArgs = (struct DFSUtilArgs *)args;
  DFSUtil(dfsArgs->vertex, dfsArgs->graphData, dfsArgs->visited,
          dfsArgs->result);
  // free(dfsArgs); // Free memory allocated for arguments
  return NULL;
}


void processNode(int currentVertex, int *next_level, int *nn, int *visited) {
  for (int i = 0; i < nodes; i++) {
    pthread_mutex_lock(&r_mutex);
    int seen = visited[i];
    if (adjMat[currentVertex][i] == 1 && !seen) {
      next_level[(*nn)++] = i;
      visited[i] = true;
    }
    pthread_mutex_unlock(&r_mutex);
  }
}

void BFSUtil(struct graph_data *graphData, int *visited,
             struct TraversalResult *result, int *current_level, int *nc,
             int *next_level, int *nn) {
  if (*nc == 0) {
    return;
  }
  pthread_t threads[*nc];
  for (int i = 0; i < *nc; i++) {
    int currentVertex = current_level[i];
    printf("\nCurrentVertex %d \n", currentVertex);
    pthread_mutex_lock(&mutex);
    visited[currentVertex] = true;
    result->vertices[result->count++] = currentVertex;
    pthread_mutex_unlock(&mutex);
    struct ProcessNodeArgs *pnArgs =
        (struct ProcessNodeArgs *)malloc(sizeof(struct ProcessNodeArgs));
    pnArgs->next_level = next_level;
    pnArgs->nn = nn;
    pnArgs->startVertex = currentVertex;
    pnArgs->visited = visited;
    if (pthread_create(&threads[i], NULL, ProcessNodeThread, (void *)pnArgs) !=
        0) {
      perror("pthread_create");
      exit(-4);
    }
  }
  // Wait for all threads to finish
  for (int t = 0; t < *nc; t++) {
    pthread_join(threads[t], NULL);
  }

  int *new_level = (int *)malloc(nodes * sizeof(int));
  if (new_level == NULL) {
    perror("Memory allocation error");
    exit(-1);
  }
  int x_new_nn = 0;
  int *new_nn = &x_new_nn;
  BFSUtil(graphData, visited, result, next_level, nn, new_level, new_nn);
}

void *BFSUtilThread(void *args) {
  struct BFSUtilArgs *bfsArgs = (struct BFSUtilArgs *)args;
  BFSUtil(bfsArgs->graphData, bfsArgs->visited, bfsArgs->result,
          bfsArgs->current_level, bfsArgs->nc, bfsArgs->next_level,
          bfsArgs->nn);
  // free(bfsArgs); // Free memory allocated for arguments
  return NULL;
}

void *ProcessNodeThread(void *args) {
  struct ProcessNodeArgs *pnArgs = (struct ProcessNodeArgs *)args;
  processNode(pnArgs->startVertex, pnArgs->next_level, pnArgs->nn,
              pnArgs->visited);
  // free(bfsArgs); // Free memory allocated for arguments
  return NULL;
}

void *TraversalProcessing(void *a) {
  struct payload *pyld = (struct payload *)a;
  printf("\npyld->shm_key= %d\n", pyld->shm_key);

  if (pyld->shm_key == -1) {
    perror("Invalid shared memory key");
    free(pyld);
    pthread_exit(NULL);
  }

  int shmid =
      shmget(pyld->shm_key, sizeof(struct graph_data), 0666 | IPC_CREAT);
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

  FILE *f = fopen(pyld->graphFile_name, "r");
  if (f != NULL) {
    // Read the number of nodes
    if (fscanf(f, "%d", &(nodes)) != 1) {
      perror("Error reading the number of nodes");
      exit(EXIT_FAILURE);
    }
    printf("node: %d\n", nodes);

    // Initialize adjacency matrix
    for (int i = 0; i < nodes; i++) {
      for (int j = 0; j < nodes; j++) {
        if (fscanf(f, "%d", &(adjMat[i][j])) != 1) {
          perror("Error reading adjacency matrix");
          exit(EXIT_FAILURE);
        }
      }
    }

    fclose(f);
  }

  for (int i = 0; i < nodes; i++) {
    for (int j = 0; j < nodes; j++) {
      printf("%d ", adjMat[i][j]);
    }
    printf("\n");
  }
  // Check the operation type (DFS or BFS)
    if (pyld->operation_number == 3) {
      struct TraversalResult *result =
          (struct TraversalResult *)malloc(sizeof(struct TraversalResult));
      // Allocate memory for the visited array
      int *visited = (int *)malloc(nodes * sizeof(int));
      if (visited == NULL) {
        perror("Memory allocation error");
        exit(-1);
      }

      // Initialize visited array to 0 (unvisited)
      for (int i = 0; i < nodes; i++) {
        visited[i] = 0;
      }
      printf("\nInitialized visited\n");

      // Initialize DFS result
      result->vertices = (int *)malloc(nodes * sizeof(int));
      if (result->vertices == NULL) {
        perror("Memory allocation error");
        exit(-1);
      };
      result->count = 0;
      printf("\nInitialized results array.\n");

      // Create arguments for DFSUtil
      // DFS(graphData->startVertex, graphData, &result);
      struct DFSUtilArgs *dfsArgs =
          (struct DFSUtilArgs *)malloc(sizeof(struct DFSUtilArgs));
      dfsArgs->vertex = graphData->startVertex-1;
      dfsArgs->graphData = graphData;
      dfsArgs->visited = visited; // Assuming 'visited' is a global array
      dfsArgs->result = result;

      // Create a thread for DFSUtil
      pthread_t t;
      if (pthread_create(&t, NULL, DFSUtilThread, (void *)dfsArgs) != 0) {
        perror("pthread_create");
        exit(-4);
      }
      // Wait for the thread to finish
      pthread_join(t, NULL);
      struct msg_buffer responseMessage;

      // You can use a loop here to send individual elements of result.vertices to
      // the client
      printf("\nResult count: %d\n", result->count);
      char s[256]; // Assuming a maximum string length of 256, adjust as needed
      s[0] = '\0'; // Initialize the string to an empty string

      for (int i = 0; i < result->count; i++) {
          printf("vertice: %d ", result->vertices[i]);

          if (result->vertices[i] != graphData->startVertex - 1) {
              char temp[16]; // Assuming a maximum integer size of 16 digits, adjust as needed
              sprintf(temp, "%d ", result->vertices[i] + 1);
              strcat(s, temp);
          }
      }
      struct msg_buffer response_msg;
      response_msg.pyld.server = pyld->server;
      response_msg.msg_type = pyld->sequence_number;
      response_msg.pyld.sequence_number = pyld->sequence_number;
      response_msg.pyld.operation_number = pyld->operation_number;
      response_msg.pyld.shm_key = pyld->shm_key;
      strcpy(response_msg.pyld.msg_text, s);
      if (msgsnd(msgqid, &response_msg, sizeof(response_msg.pyld), 0) == -1) {
        perror("Error in msgsnd");
      }

      printf("\nWork Done!\n");
      // Free memory used by DFS result
      free(result->vertices);

    }
// Check the operation type (DFS or BFS)
  else if (pyld->operation_number == 4) {
    struct TraversalResult *result =
        (struct TraversalResult *)malloc(sizeof(struct TraversalResult));
    // Allocate memory for the visited array
    int *visited = (int *)malloc(nodes * sizeof(int));
    if (visited == NULL) {
      perror("Memory allocation error");
      exit(-1);
    }

    // Initialize visited array to 0 (unvisited)
    for (int i = 0; i < nodes; i++) {
      visited[i] = 0;
    }
    // Initialize BFS result
    int *next_level = (int *)malloc(nodes * sizeof(int));
    if (next_level == NULL) {
      perror("Memory allocation error");
      exit(-1);
    }
    int nn_x = 0;
    int *nn = &nn_x;
    int nc_x = 1;
    int *nc = &nc_x;
    int *current_level = (int *)malloc(nc_x * sizeof(int));
    if (current_level == NULL) {
      perror("Memory allocation error");
      exit(-1);
    }
    current_level[0] = graphData->startVertex-1;
    result->vertices = (int *)malloc(nodes * sizeof(int));
    if (result->vertices == NULL) {
      perror("Memory allocation error");
      exit(-1);
    }
    result->count = 0;
    // Create new BFSArgs for the BFS call
    struct BFSUtilArgs *bfsArgs =
        (struct BFSUtilArgs *)malloc(sizeof(struct BFSUtilArgs));
    bfsArgs->graphData = graphData;
    bfsArgs->visited = visited;
    bfsArgs->result = result;
    bfsArgs->current_level = current_level;
    bfsArgs->nc = nc;
    bfsArgs->next_level = next_level;
    bfsArgs->nn = nn;
    // Loop over all nodes to find paths starting from each node

    // Create a new thread for each unvisited node
    pthread_t t;
    printf("\nStarting BFS!\n******\n");
    if (pthread_create(&t, NULL, BFSUtilThread, (void *)bfsArgs) != 0) {
      perror("pthread_create");
      exit(-4);
    }

    // Wait for the thread to complete
    pthread_join(t, NULL);

    // Send the BFS result back to the client
    struct msg_buffer responseMessage;
    // You can use a loop here to send individual elements of result.vertices to
    // the client
    printf("\nResult count: %d\n", result->count);
    char s[256]; // Assuming a maximum string length of 256, adjust as needed
    s[0] = '\0'; // Initialize the string to an empty string

    for (int i = 0; i < result->count; i++) {
        printf("vertice: %d ", result->vertices[i]);
            char temp[16]; // Assuming a maximum integer size of 16 digits, adjust as needed
            sprintf(temp, "%d ", result->vertices[i] + 1);
            strcat(s, temp);
    }
    struct msg_buffer response_msg;
    response_msg.pyld.server = pyld->server;
    response_msg.msg_type = pyld->sequence_number;
    response_msg.pyld.sequence_number = pyld->sequence_number;
    response_msg.pyld.operation_number = pyld->operation_number;
    response_msg.pyld.shm_key = pyld->shm_key;
    strcpy(response_msg.pyld.msg_text, s);
    if (msgsnd(msgqid, &response_msg, sizeof(response_msg.pyld), 0) == -1) {
      perror("Error in msgsnd");
    }

    printf("\nWork Done!\n");
    // Free memory used by DFS result
    free(result->vertices);
  }

  shmdt(graphData);
  free(pyld); // Free allocated memory here

  // Release the semaphore to signal completion
  sem_post(semaphore);

  pthread_exit(NULL);
}


int main(int argc, char *argv[]) {
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

  printf("Secondary Server: Connected to Message Queue with ID %d\n", msgqid);
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <server_number>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  int serverNumber = atoi(argv[1]);
  semaphore = sem_open("semap", O_CREAT, 0644, 0);
  if (semaphore == SEM_FAILED) {
    perror("Semaphore fail");
    exit(EXIT_FAILURE);
  }
  
  pthread_mutex_init(&mutex, NULL);
  
  while (1) {
    if(serverNumber == 2){
    if (msgrcv(msgqid, &msg, sizeof(msg.pyld), 2, 0) == -1) {
      perror("msgrcv");
      exit(-3);
    }
    else {
      pthread_t t;
      struct payload *pyld = malloc(sizeof(struct payload));
      memcpy(pyld, &(msg.pyld), sizeof(struct payload));
      pyld->shm_key = msg.pyld.shm_key;
      strcpy(pyld->graphFile_name, msg.pyld.graphFile_name);

      printf("\n2.file name: %s", msg.pyld.graphFile_name);

      if (pthread_create(&t, NULL, TraversalProcessing, (void *)pyld) != 0) {
        perror("pthread_create");
        exit(-4);
      }

      // Wait for the thread to complete
      pthread_join(t, NULL);
    }
  }
    else if(serverNumber == 3){
      if (msgrcv(msgqid, &msg, sizeof(msg.pyld), 3, 0) == -1) {
        perror("msgrcv");
        exit(-3);
      }
      else {
        pthread_t t;
        struct payload *pyld = malloc(sizeof(struct payload));
        memcpy(pyld, &(msg.pyld), sizeof(struct payload));
        pyld->shm_key = msg.pyld.shm_key;
        strcpy(pyld->graphFile_name, msg.pyld.graphFile_name);

        printf("\n2.file name: %s", msg.pyld.graphFile_name);

        if (pthread_create(&t, NULL, TraversalProcessing, (void *)pyld) != 0) {
          perror("pthread_create");
          exit(-4);
        }

        // Wait for the thread to complete
        pthread_join(t, NULL);
      }
    }
  }

  // Detach shared memory (moved outside the loop)
  if (shmdt(graphData) == -1) {
    perror("shmdt");
  }

  sem_close(semaphore);
  sem_unlink("semap");
  pthread_mutex_destroy(&mutex);

  return 0;
}