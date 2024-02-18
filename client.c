#include "utils.h"

int sequenceNumber;
int counter = 0;
int MAX_NODES = 30;

int main(void) {
  struct msg_buffer msg;

  // Connecting to the message queue created by the load balancer
  key_t key = ftok("./loadBalancer.c", MSG_KEY);
  if (key == -1) {
    perror("ftok");
    exit(-1);
  }

  int msgqid = msgget(key, 0666);
  if (msgqid == -1) {
    perror("msgget");
    exit(-2);
  }

  printf("Client: Connected to Message Queue with ID %d\n", msgqid);

  // Continuously interact with the load balancer
  printf("1. Add a new graph to the database\n");          // primary server
  printf("2. Modify an existing graph of the database\n"); // primary server
  printf("3. Perform DFS on an existing graph of the database\n"); // secondary
                                                                   // server
  printf("4. Perform BFS on an existing graph of the database\n"); // secondary
                                                                   // server

  while (1) {
    counter++;
    printf("Enter Sequence Number ");
    if (scanf("%d", &sequenceNumber) != 1 || sequenceNumber != counter) {
      perror("Error reading sequence number");
      exit(-1);
    }

    printf("Enter Operation Number ");
    if (scanf("%d", &(msg.pyld.operation_number)) != 1) {
      perror("Error reading operation number");
      exit(-1);
    }

    printf("Enter Graph File Name ");
    if (scanf("%s", msg.pyld.graphFile_name) != 1) {
      perror("Error reading graph file name");
      exit(-1);
    }

    // Generate a unique key for the shared memory segment per response
    key_t shkey = ftok("./loadBalancer.c", (msgqid + counter));
    if (shkey == -1) {
      perror("ftok");
      exit(-4);
    }

    msg.pyld.shm_key = shkey;

    // Create or connect to the shared memory segment
    int shmid = shmget(shkey, sizeof(struct graph_data), 0666 | IPC_CREAT);
    if (shmid == -1) {
      perror("shmget");
      exit(-5);
    }

    // Attach shared memory to the process
    struct graph_data *graphData = (struct graph_data *)shmat(shmid, NULL, 0);
    if (graphData == (void *)-1) {
      perror("shmat");
      printf("shmat errno: %d\n", errno);
      exit(-1);
    }

    if (msg.pyld.operation_number == 1 || msg.pyld.operation_number == 2) {
        printf("Enter number of nodes of the graph ");
        scanf("%d", &(graphData->nodes));

      for (int i = 0; i < MAX_NODES; i++) {
          for (int j = 0; j < MAX_NODES; j++) {
              graphData->adjacencyMatrix[i * MAX_NODES + j] = 0;
          }
      }

        // Initialize the adjacency matrix
      printf("Enter number of nodes of the graph. Enter adjacency matrix, each row on a separate line and elements of a single row separated by whitespace characters");
      for (int i = 0; i < graphData->nodes; i++) {
          for (int j = 0; j < graphData->nodes; j++) {
              scanf("%d", &(graphData->adjacencyMatrix[i * MAX_NODES + j]));
          }
      }
    }
 else if (msg.pyld.operation_number == 3 ||
               msg.pyld.operation_number == 4) {
      printf("Enter starting vertex ");
      if (scanf("%d", &(graphData->startVertex)) != 1) {
        perror("Error reading starting vertex");
        exit(-1);
      }
    }

    // Send the message to the load balancer
    if(msg.pyld.operation_number == 1 || msg.pyld.operation_number == 2){
    msg.msg_type = msg.pyld.sequence_number;
    msg.pyld.server = 1;
    }  
    else if(msg.pyld.operation_number == 3 || msg.pyld.operation_number == 4){
      msg.msg_type = msg.pyld.sequence_number;
      msg.pyld.server = 2;
      }
    printf("\npyld->shm_key= %d\n", msg.pyld.shm_key);
    if (msgsnd(msgqid, &msg, sizeof(msg.pyld), 0) == -1) {
      perror("Error in msgsnd");
      exit(-6);
    }

    // Wait for the load balancer to send a response
    struct msg_buffer reply;
    if (msgrcv(msgqid, &reply, sizeof(reply.pyld), 0, 0) == -1) {
      perror("Error receiving response from server");
      exit(EXIT_FAILURE);
    }
    if(reply.pyld.sequence_number == msg.pyld.sequence_number)
    printf("%s\n", reply.pyld.msg_text);

    // Detach shared memory
    if (shmdt(graphData) == -1) {
      perror("shmdt");
      exit(-10);
    }
  }

  return 0;
}
