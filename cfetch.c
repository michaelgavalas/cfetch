#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* 200809L targets the modern POSIX.1-2008 standard */
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(void)
{
  const char *host = "neverssl.com";
  const char *port = "80";

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

  // Format the raw HTTP request
  char request[512];
  int request_len = snprintf(
      request,
      sizeof(request),
      "GET / HTTP/1.1\r\n"
      "host: %s\r\n"
      "User-Agent: RawCSocketClient/1.0\r\n"
      "Connection: close\r\n"
      "\r\n",
      host);

  if (request_len < 0 || (size_t)request_len >= sizeof(request))
  {
    fprintf(stderr, "Request string was truncated or failed to format\n");
    close(sockfd);
    return -1;
  }

  // Send the bytes over the network
  ssize_t bytes_sent = send(sockfd, request, request_len, 0);
  if (bytes_sent == -1)
  {
    perror("Failed to send request");
    close(sockfd);
    return -1;
  }

  printf("Sent %zd bytes to the server:\n\n%s\n", bytes_sent, request);

  // Create a 4KB chunk buffer
  char buffer[4096];
  ssize_t bytes_received;

  printf("--- Server Response Start ---\n");

  // Loop until recv() returns 0 (connection closed) or -1 (error)
  while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    // Append \0 to the end of the string
    buffer[bytes_received] = '\0';

    // Print the chunk to stdout
    printf("%s", buffer);
  }

  printf("--- Server Response Ended ---\n");

  if (bytes_received == -1)
  {
    perror("recv failed");
    close(sockfd);
    return -1;
  }

  printf("Connection closed cleanly by the server\n");

  // Close the socket when finished
  close(sockfd);

  return 0;
}
