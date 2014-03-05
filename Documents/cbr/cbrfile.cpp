#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
using namespace std;

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 25064

#define PACKET_SIZE 2048
#define PACKET_INTERVAL 10

void usage(char* name) {
  printf("Sender Usage: %s -s [-ip IP] [-p Port] [-f Filename]\n", name);
  printf("Receiver Usage: %s -r [-p Port]\n", name);
  printf("Default IP - %s\n", DEFAULT_IP);
  printf("Default Port - %i\n", DEFAULT_PORT);
  exit(0);
}

/* SENDER */

void initsender(string ip, int port, string filename) {
  struct sockaddr_in own_addr, other_addr;
  socklen_t addr_size = sizeof(own_addr);
  struct hostent *other;
  int sfd;
  int msgctr = 1;
  FILE *fp;

  char buffer[PACKET_SIZE];

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

  // send file info
  struct stat st;
  if (stat(filename.c_str(), &st) == -1) {
    fprintf(stderr, "ERROR: Cannot open file %s", filename.c_str());
    exit(1);
  }

  stringstream info_ss;
  info_ss << "FILE " << filename << " " << st.st_size;
  string info = info_ss.str();
  int sent = sendto(sfd, info.c_str(), info.size(), 0, (struct sockaddr *)&other_addr, addr_size);
  if (sent < 0) {
    fprintf(stderr, "ERROR: Failed to send file info to receiver\n");
    exit(1);
  }

  printf("Sent download notification to %s for file %s (%li bytes)\n", ip.c_str(), filename.c_str(), st.st_size);
 
  // open file to transmit
  fp = fopen(filename.c_str(), "rb+");
  if(fp == NULL) {
    fprintf(stderr, "ERROR: File not found\n");
    exit(1);
  }  
  
  while(!feof(fp)){
    usleep(PACKET_INTERVAL);
    memset(&buffer, 0, PACKET_SIZE);
    int read = fread(buffer, 1, PACKET_SIZE, fp);
    if (read == 0)
      break;
    int fsent = sendto(sfd, buffer, read, 0, (struct sockaddr *)&other_addr, addr_size);
    if (fsent < 0) {
      if (errno == EMSGSIZE)
        fprintf(stderr, "ERROR: Message too long to send\n");
      else
        fprintf(stderr, "ERROR: sendto() failed (%d)\n", errno);
      exit(1);
    }

    printf("Sent %d bytes to receiver\n", fsent);
  }

  printf("\nDone.\n");
}

/* RECEIVER */

void initreceiver(int port) {
  struct sockaddr_in own_addr, other_addr;
  socklen_t addr_size = sizeof(own_addr);
  int sfd;
  int rcvctr = 1;
  socklen_t other_size = sizeof(other_addr);
  char other_ip[INET_ADDRSTRLEN];

  char buffer[PACKET_SIZE];

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

  memset(&buffer, 0, PACKET_SIZE);
  printf("Listening on port %d for incoming file transfer...\n", port);
  // listen for file info packet
  int recv = recvfrom(sfd, buffer, PACKET_SIZE, 0,
                      (struct sockaddr *) &other_addr, &addr_size);
  if (recv == -1) {
    fprintf(stderr, "ERROR: Error receiving file info\n");
    exit(1);
  }
 
  inet_ntop(AF_INET, &(other_addr.sin_addr), other_ip, INET_ADDRSTRLEN);

  // parse file transfer notification
  stringstream ss((string(buffer)));
  string request_type, filename, filesize_s;
  std::getline(ss, request_type, ' ');
  std::getline(ss, filename, ' ');
  std::getline(ss, filesize_s, ' ');
  int filesize = atoi(filesize_s.c_str());
  if (request_type != "FILE" || filename == "" || filesize == 0) {
    fprintf(stderr, "ERROR: Sender did not instantiate a file transfer\n");
    exit(1);
  }
  printf("Receiving file transfer from %s:%d: %s (%d bytes)\n",
        other_ip, ntohs(other_addr.sin_port), filename.c_str(), filesize);

  // download file from sender
  stringstream filename_dest;
  filename_dest << "download/" << filename;
  FILE *fp = fopen(filename_dest.str().c_str(), "wb");

  int total = 0;
  while (total < filesize) {
    memset(&buffer, 0, PACKET_SIZE);
    int count = recvfrom(sfd, buffer, PACKET_SIZE, 0,
                        (struct sockaddr *) &other_addr, &addr_size);
    if (count == -1) {
      fprintf(stderr, "ERROR: Error receiving file part\n");
      exit(1);
    }
    if (count == 0)
      break;
 
    inet_ntop(AF_INET, &(other_addr.sin_addr), other_ip, INET_ADDRSTRLEN);
   
    fwrite(buffer, 1, count, fp);
 
    total += count;
    printf("Got %d bytes from %s:%d (total: %d)\n", count, other_ip, ntohs(other_addr.sin_port), total); 
  }

  fclose(fp);
  printf("\nDone.\n"); 
}

/* MAIN */

int main(int argc, char **argv) {
  bool sender = false, receiver = false;
  string ip = "127.0.0.1";
  int port = 20064;
  string filename;

  // parse argments
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-s"))
      sender = true;
    else if (!strcmp(argv[i], "-r"))
      receiver = true;
    else if (!strcmp(argv[i], "-ip")) {
      ip = string(argv[i+1]);
      i++;
    }
    else if (!strcmp(argv[i], "-p")) {
      port = atoi(argv[i+1]);
      i++;
    }
    else if (!strcmp(argv[i] , "-f") && i<argc-1) {
      filename = argv[i+1];
      i++;
    }      
    else
      usage(argv[0]); 
  }
  if (sender == receiver)
    usage(argv[0]);

  if (sender) {
    initsender(ip, port, filename);
  }
  else if (receiver) {
    initreceiver(port);
  }

  return 0;
}
