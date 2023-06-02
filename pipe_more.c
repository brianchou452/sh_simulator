#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  int fd[2];
  pid_t pid;

  if (pipe(fd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // Child process
    close(fd[0]);  // Close unused read end

    // Redirect stdout to pipe write end
    if (dup2(fd[1], STDOUT_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    // Execute cat command
    execlp("cat", "cat", "animal.txt", NULL);
    perror("execlp");
    exit(EXIT_FAILURE);
  } else {
    // Parent process
    close(fd[1]);  // Close unused write end

    // Redirect stdin to pipe read end
    if (dup2(fd[0], STDIN_FILENO) == -1) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    printf("Pipe read end redirected to stdin\n");

    // Execute more command
    execlp("more", "more", NULL);
    perror("execlp");
    exit(EXIT_FAILURE);
  }

  return 0;
}