// STEFAN MIRUNA ANDREEA 324CA
#include "common.hpp"

#include <sys/socket.h>
#include <sys/types.h>

// the following 2 functions have been inspired from the 7th lab

int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while(bytes_remaining) {
    int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
    if (rc <= 0) {
      return rc;
    }
    bytes_received += rc;
    bytes_remaining -= rc;
  }

  return bytes_received;
}


int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

  while(bytes_remaining) {
    int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
    if (rc <= 0) {
      return rc;
    }
    bytes_sent += rc;
    bytes_remaining -= rc;
  }
  return bytes_sent;
}