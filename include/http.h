#ifndef CFETCH_HTTP_H
#define CFETCH_HTTP_H

int http_send_get(int sockfd, const char *host, const char *path);

int http_receive_response(int sockfd, const char *output_filepath);

#endif
