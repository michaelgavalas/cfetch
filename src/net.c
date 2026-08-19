#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

int net_connect(const char *host, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *res;

  // Zero out the struct, clear garbage data
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       // Request IPv4
  hints.ai_socktype = SOCK_STREAM; // Request TCP

  // Perform the DNS lookup
  int status = getaddrinfo(host, port, &hints, &res);
  if (status != 0)
  {
    fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(status));
    return -1;
  }

  printf("Successfully resolved host %s\n", host);

  char ip_string[INET_ADDRSTRLEN];

  struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;

  inet_ntop(AF_INET, &(ipv4->sin_addr), ip_string, sizeof(ip_string));

  printf("The IP address is %s\n", ip_string);

  // Create the socket using the blueprint from 'res'
  int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd == -1)
  {
    perror("failed to create socket");
    freeaddrinfo(res); // Clean up memory before exiting
    return -1;
  }

  printf("Socket descriptor created: %d\n", sockfd);

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
  {
    perror("failed to connect to host");
    close(sockfd);     // Close the socket to avoid leaking resources
    freeaddrinfo(res); // Free the linked list
    return -1;
  }

  printf("Successfully connected to %s port %s\n", host, port);

  // We are connected, we don't need 'res' anymore, so we free it
  freeaddrinfo(res);

  return sockfd;
}
