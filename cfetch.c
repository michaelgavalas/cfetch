#include <stdio.h>

#define MAX_CHAR_LIMIT 50

int main(void)
{
  char username[MAX_CHAR_LIMIT];

  int username_length = sizeof(username) / sizeof(username[0]);

  printf("Enter TikTok username: ");
  fgets(username, MAX_CHAR_LIMIT, stdin);

  printf("Username entered: %s", username);

  return 0;
}
