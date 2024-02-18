#include "utils.h"

int msgqid;
int termination = 0;


void forwardPrimaryServer(struct payload *pyld) {
  struct msg_buffer response_msg;
  response_msg.msg_type = 1;
  response_msg.pyld.server = 1;
  response_msg.pyld.server = pyld->server;
  response_msg.pyld.sequence_number = pyld->sequence_number;
  response_msg.pyld.operation_number = pyld->operation_number;
  strcpy(response_msg.pyld.graphFile_name, pyld->graphFile_name);
  strcpy(response_msg.pyld.msg_text, "Sent by Load Balancer\n");

  response_msg.pyld.shm_key = pyld->shm_key;
  if (msgsnd(msgqid, &response_msg, sizeof(response_msg.pyld), 0) == -1) {
    perror("msgsnd");
    exit(-1);
  }
}
void forwardSecondaryServer(struct payload *pyld) {
  struct msg_buffer response_msg;
  if(pyld->sequence_number%2 == 0)
  {
    response_msg.msg_type = 3;
  }
  else{
  response_msg.msg_type = 2;
  }
  response_msg.pyld.server = 2;
  response_msg.pyld.sequence_number = pyld->sequence_number;
  response_msg.pyld.operation_number = pyld->operation_number;
  strcpy(response_msg.pyld.graphFile_name, pyld->graphFile_name);
  strcpy(response_msg.pyld.msg_text, "Sent by Load Balancer\n");

  response_msg.pyld.shm_key = pyld->shm_key;
  if (msgsnd(msgqid, &response_msg, sizeof(response_msg.pyld), 0) == -1) {
    perror("msgsnd");
    exit(-1);
  }
}
int main(void) {
  struct msg_buffer msg;
  key_t key;

  char bufff[100];
  int shmid;

  // Creating a message queue
  key = ftok("./loadBalancer.c", MSG_KEY);
  if (key == -1) {
    perror("ftok");
    exit(-1);
  }

  msgqid = msgget(key, 0666 | IPC_CREAT);
  if (msgqid == -1) {
    perror("msgget");
    exit(-2);
  }
  printf("Load Balancer: Message Queue Created with ID %d\n", msgqid);
  // Continuously listen for messages
  while (1) {

    // Receive a message
    if (msgrcv(msgqid, &msg, sizeof(msg.pyld), 0, 0) == -1) {
      perror("msgrcv");
      exit(-3);
    } else {
      if (msg.pyld.operation_number == 1 || msg.pyld.operation_number == 2) {
        forwardPrimaryServer(&msg.pyld);
      } else {
        // write for option 3 and 4, to secondary servers
        if (msg.pyld.operation_number == 3 || msg.pyld.operation_number == 4) {
          forwardSecondaryServer(&msg.pyld);
        }
      }
    }
  }

  return 0;
}
