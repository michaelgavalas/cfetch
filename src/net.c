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
#include <sys/time.h>

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

  int sockfd = -1;
  struct addrinfo *p;

  // Loop through the addresses in the linked list and attempt to connect to each one until one succeeds
  for (p = res; p != NULL; p = p->ai_next)
  {
    // Print the IP address we are attempting to connect to
    char ip_string[INET_ADDRSTRLEN];
    struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
    inet_ntop(AF_INET, &(ipv4->sin_addr), ip_string, sizeof(ip_string));
    printf("Trying IP address: %s\n", ip_string);

    // Attempt to create the socket
    int sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1)
    {
      perror("failed to create socket for this IP address, trying next");
      continue;
    }

    printf("Socket descriptor created: %d\n", sockfd);

    // Temporary workaround for connection with a 5 sec timeout
    // will replace with non-blocking I/O (basically async/await in higher level languages).
    // TODO: replace timeout with non-blocking I/O
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1)
    {
      perror("failed to connect to host");
      close(sockfd); // Close the socket to avoid leaking resources
      sockfd = -1;   // Reset sockfd to -1 to indicate not connected
      continue;
    }

    // If we reached here it means the connection was successful
    printf("Successfully connected to %s (%s) on port %s\n", host, ip_string, port);
    break;
  }

  // We are finished with the address list, we don't need 'res' anymore, so we free it
  freeaddrinfo(res);

  if (p == NULL)
  {
    fprintf(stderr, "Failed to connect to any address for %s\n", host);
    return -1;
  }

  return sockfd;
}
