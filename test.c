#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
  int fd[2];
  int pid;

  if (pipe(fd) == -1) {
    perror("pipe");
    exit(1);
  }

  pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(1);
  } else if (pid == 0) {
    // Child process
    close(fd[1]);
    dup2(fd[0], STDIN_FILENO); /*p1 will close after STDIN receive EOF*/
    close(fd[0]);
    execlp("cat", "cat", "logger.h", NULL);
    exit(0);
  } else {
    // Parent process
    close(fd[0]);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);
    execlp("more", "more", NULL);  // Execute wc command
    exit(0);
  }

  return 0;
}

int recursive_pipe(int n) {
  // char current_command[1024];
  // strcpy(current_command, commands[ncommands - n - 1]);

  if (pipe(fd1) < 0) {
    perror("pipe");
    exit(1);
  }
  if (pipe(fd2) < 0) {
    perror("pipe");
    exit(1);
  }
  LOG_DEBUG("n = %d", n);

  pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  } else if (pid == 0) {
    // Child process
    if (n == 0) {
      close(fd1[1]);
      dup2(fd1[0], STDIN_FILENO); /*p1 will close after STDIN receive EOF*/
      close(fd1[0]);
      close(fd2[0]);
      close(fd2[1]);
      dup2(stdout_copy, STDOUT_FILENO);
      LOG_DEBUG("executing command: %s", commands[ncommands - n - 1]);
      exec_command(commands[ncommands - n - 1]);
      exit(0);
      return 0;
    } else {
      close(fd1[1]);
      dup2(fd1[0], STDIN_FILENO); /*p1 will close after STDIN receive EOF*/
      close(fd1[0]);
      close(fd2[0]);
      dup2(fd2[1], STDOUT_FILENO);
      close(fd2[1]);
      LOG_DEBUG("executing command: %s", commands[ncommands - n - 1]);
      exec_command(commands[ncommands - n - 1]);
      recursive_pipe(n - 1);
      exit(0);
    }

  } else {
    // Parent process
    close(fd1[0]);
    dup2(fd1[1], STDOUT_FILENO);
    close(fd1[1]);
    // std::cout << pipe_buff << std::flush;
    dup2(stdout_copy,
         STDOUT_FILENO); /*p1 write end isn't used anymore, send EOF*/
    close(fd2[1]);
    // if (i < cmd.size() - 1) {
    //   memset(pipe_buff, 0, sizeof(pipe_buff));
    //   read(p2[0], pipe_buff, sizeof(pipe_buff));
    // }
    close(fd2[0]); /*p2 close here*/
    pid = wait(&status);
  }

  /*char *cmd;

  tokenize(command);  // divide line into token strings

  cmd = args[0];

  if (strcmp(cmd, "cd") == 0) {
    chdir(args[1]);
    return 0;
  }
  if (strcmp(cmd, "exit") == 0) exit(0);

  /*fork()調用返回兩次，一次在父進程中，一次在子進程中。
  在父進程中，fork()返回子進程的PID，而在子進程中，fork()返回0。
  這樣，父進程可以通過檢查fork()的返回值來確定它是否正在運行子進程。*/
  /*pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(1);
  } else if (pid != 0) {
    LOG_DEBUG("mysh %d forked a child sh %d", getpid(), pid);
    LOG_DEBUG("mysh %d wait for child sh %d to terminate", getpid(), pid);
    pid = wait(&status);
    LOG_DEBUG("ZOMBIE child=%d exitStatus=%x", pid, status);
    LOG_DEBUG("mysh %d repeat loop", getpid());
  } else {
    fflush(stdout);

    if (redirect_stdout) {
      close(1);
      open(redirect_stdout_filename, O_WRONLY | O_CREAT, 0644);
    }

    int r = execvp(cmd, args);
    if (r < 0) {
      perror("execlp failed");
    }

    exit(1);
  }*/
  return 0;
}