#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* 200809L targets the modern POSIX.1-2008 standard */
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

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
  ssize_t bytes_written = 0;

  // Declare state variables to check whether we are past the header bytes (pure body bytes)
  int headers_passed = 0;
  char *header_end = NULL;

  // Create the 'output' folder
  // 0755 gives rwx to owner, rx to others
  if (mkdir("output", 0755) == -1)
  {
    if (errno != EEXIST)
    {
      perror("Failed to create 'output' folder");
      close(sockfd);
      return -1;
    }
  }

  // Open a file for writing, create it if it doesn't exist, truncate it if it does.
  // 0644 gives Read/Write permission to owner, Read-only to everyone else.
  int file_fd = open("./output/output.html", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (file_fd == -1)
  {
    perror("Failed to open output file");
    close(sockfd);
    return -1;
  }

  printf("--- Server Response Start ---\n");

  // Loop until recv() returns 0 (connection closed) or -1 (error)
  while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0)
  {
    // Append \0 to the end of the string
    buffer[bytes_received] = '\0';

    if (!headers_passed) // Runs before we find the end of the headers: "\r\n\r\n"
    {
      // Search for \r\n\r\n (headers end) in the current buffer
      header_end = strstr(buffer, "\r\n\r\n");

      if (header_end != NULL) // Means we found the end of the headers
      {
        // Mark headers as done
        headers_passed = 1;

        // Calculate where body starts
        char *body_start = header_end + 4;

        // Calculate how many body bytes are in this buffer
        // (Pointer arithmetic: body_start - buffer gives us how many header bytes were skipped)
        size_t header_bytes = (size_t)(body_start - buffer);
        size_t body_bytes = (size_t)bytes_received - header_bytes;

        // Write bytes starting at body_start
        ssize_t bytes_written_chunk = write(file_fd, body_start, body_bytes);

        bytes_written += bytes_written_chunk;

        printf("Header bytes: %zu\nBody bytes: %zu\n", header_bytes, body_bytes);
      }
    }
    else // Pure body mode (every single byte is payload data)
    {
      ssize_t bytes_written_chunk = write(file_fd, buffer, bytes_received);

      bytes_written += bytes_written_chunk;
    }

    // Print the chunk to stdout
    printf("%s", buffer);
  }

  printf("--- Server Response Ended ---\n");

  close(file_fd);

  if (bytes_received == -1)
  {
    perror("recv failed");
    close(sockfd);
    return -1;
  }

  if (bytes_written > 0)
  {
    printf("Total bytes written: %zd\n", bytes_written);
  }

  printf("Connection closed cleanly by the server\n");

  // Close the socket when finished
  close(sockfd);

  return 0;
}
