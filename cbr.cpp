#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>
using namespace std;

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 25064

#define PACKET_SIZE     64
#define PACKET_SIZE_MAX 1024
#define PACKET_RECV_MAX 10
#define PACKET_INTERVAL 1000000 // 1 second

void usage(char* name) {
  printf("Sender Usage: %s -s [-ip IP] [-p Port]\n", name);
  printf("Receiver Usage: %s -r [-p Port]\n", name);
  printf("Default IP - %s\n", DEFAULT_IP);
  printf("Default Port - %i\n", DEFAULT_PORT);
  exit(0);
}

/* SENDER */

void initsender(string ip, int port) {
  struct sockaddr_in own_addr, other_addr;
  socklen_t addr_size = sizeof(own_addr);
  struct hostent *other;
  int sfd;

  char buffer[PACKET_SIZE_MAX];

  if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    fprintf(stderr, "ERROR: Unable to create UDP socket\n");
    exit(1);
  }

  other = gethostbyname(ip.c_str());
  if (other == NULL) {
    fprintf(stderr, "ERROR: No such host\n");
    exit(1);
  }
  
  // configure own address 
  memset(&own_addr, 0, addr_size);
  own_addr.sin_family = AF_INET;
  own_addr.sin_port = 0;
  own_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sfd, (struct sockaddr *) &own_addr, addr_size) == -1) {
    fprintf(stderr, "ERROR: Unable to bind to UDP socket\n");
    exit(1);
  }

  // configure other address
  memset(&other_addr, 0, addr_size);
  other_addr.sin_family = AF_INET;
  bcopy((char *)other->h_addr, (char *)&other_addr.sin_addr.s_addr, other->h_length);
  other_addr.sin_port = htons(port);

  // Send example message
  memset(&buffer, 0, PACKET_SIZE_MAX);
  //strcpy(buffer, "Data goes here");
  //int msglen = strlen(buffer);
  int msglen = PACKET_SIZE;
  for (int i = 0; i < PACKET_RECV_MAX; i++) {
    int count = sendto(sfd, buffer, msglen, 0, 
                       (struct sockaddr *) &other_addr, addr_size);
    if (count == -1) {
      fprintf(stderr, "ERROR: Failed to send message\n");
      exit(1);
    }
    printf("Sent message of length %d to %s:%d\n", msglen, ip.c_str(), port);
    
    // Sleep
    usleep(PACKET_INTERVAL);
  }

  printf("\nDone.\n");
}

/* RECEIVER */

void initreceiver(int port) {
  struct sockaddr_in own_addr, other_addr;
  socklen_t addr_size = sizeof(own_addr);
  char other_ip[INET_ADDRSTRLEN];
  int sfd;

  char buffer[PACKET_SIZE_MAX];

  if ((sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
    fprintf(stderr, "ERROR: Unable to create UDP socket\n");
    exit(1);
  }

  // configure own address
  memset(&own_addr, 0, addr_size);
  own_addr.sin_family = AF_INET;
  own_addr.sin_port = htons(port);
  own_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(sfd, (struct sockaddr *) &own_addr, addr_size) == -1) {
    fprintf(stderr, "ERROR: Unable to bind to UDP socket\n");
    exit(1);
  }

  // listen for incoming packets
  printf("Listening on port %d for incoming packets...\n", port);
  for (int i = 0; i < PACKET_RECV_MAX; i++) {
    memset(&buffer, 0, PACKET_SIZE_MAX);
    int count = recvfrom(sfd, buffer, PACKET_SIZE_MAX, 0,
                         (struct sockaddr *) &other_addr, &addr_size);
    if (count == -1) {
      fprintf(stderr, "ERROR: Failed to receive packet from sender\n");
      exit(1);
    }

    inet_ntop(AF_INET, &(other_addr.sin_addr), other_ip, INET_ADDRSTRLEN); 

    printf("Received %d bytes from %s:%d\n", count, other_ip, ntohs(other_addr.sin_port));
  }

  printf("\nDone.\n");
}

/* MAIN */

int main(int argc, char **argv) {
  bool sender = false, receiver = false;
  string ip = "127.0.0.1";
  int port = 20064;

  // parse argments
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s"))
      sender = true;
    else if (!strcmp(argv[i], "-r"))
      receiver = true;
    else if (!strcmp(argv[i], "-ip") && i < argc-1) {
      ip = string(argv[i+1]);
      i++;
    }
    else if (!strcmp(argv[i], "-p")) {
      port = atoi(argv[i+1]);
      i++;
    } else
      usage(argv[0]); 
  }
  if (sender == receiver)
    usage(argv[0]);

  if (sender) {
    initsender(ip, port);
  }
  else if (receiver) {
    initreceiver(port);
  }

  return 0;
}
