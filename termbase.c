#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "logger.h"

#define Close(FD)                                                          \
  do {                                                                     \
    int Close_fd = (FD);                                                   \
    if (close(Close_fd) == -1) {                                           \
      perror("close");                                                     \
      LOG_ERROR("%s:%d: close(" #FD ") %d", __FILE__, __LINE__, Close_fd); \
    }                                                                      \
  } while (0)

char *args[64];  // token string pointers

int pid, status;

bool redirect_stdout = false;
char *redirect_stdout_filename;
bool redirect_stdin = false;
char *redirect_stdin_filename;

char *commands[64];
int ncommands = 0;

int stdin_copy = 0;
int stdout_copy = 0;
int fd1[2];
int fd2[2];

/*刪除 command 前後多餘的空格*/
void trim_white_space(char *str) {
  int start = 0;
  int end = strlen(str) - 1;
  while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n') {
    start++;
  }
  while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
    end--;
  }
  str[end + 1] = '\0';
  strcpy(str, str + start);
}

/*
以空白分割指令
divide line into token strings
*/
int tokenize(char *pathname) {
  int n;  // number of token strings
  char *s;
  redirect_stdout = false;  // reset
  redirect_stdin = false;
  char gpath[128];
  strcpy(gpath, pathname);  // copy 一份才不會改到原本的
  s = strtok(gpath, " ");
  n = 0;
  while (s) {
    args[n] = malloc(strlen(s) + 1);
    if (strcmp(s, ">") == 0) {
      redirect_stdout = true;
      s = strtok(0, " ");
      redirect_stdout_filename = malloc(strlen(s) + 1);
      strcpy(redirect_stdout_filename, s);
      LOG_DEBUG("redirect stdout to %s", redirect_stdout_filename);
      break;
    } else if (strcmp(s, "<") == 0) {
      redirect_stdin = true;
      s = strtok(0, " ");
      redirect_stdin_filename = malloc(strlen(s) + 1);
      strcpy(redirect_stdin_filename, s);
      LOG_DEBUG("redirect stdin from %s", redirect_stdin_filename);
      break;
    }
    strcpy(args[n], s);
    n++;
    s = strtok(0, " ");
  }
  args[n] = NULL;

  for (char **p = args; *p != NULL; p++) {
    LOG_DEBUG("args[%ld] = %s", p - args, *p);
  }
  LOG_DEBUG("args[%d] = %s", n, args[n]);
  return 0;
}

// print argc, argv and env
void print_debug_info(int argc, char *argv[], char *env[]) {
  char *dir[64];  // dir string pointers
  int ndir = 0;   // number of dirs
  LOG_DEBUG("argc: %d", argc);
  for (int i = 0; i < argc; i++) {
    LOG_DEBUG("argv[%d]: %s", i, argv[i]);
  }
  for (int i = 0; env[i] != NULL; i++) {
    LOG_DEBUG("env[%d]: %s", i, env[i]);
  }

  char *path = getenv("PATH");
  if (path != NULL) {
    LOG_DEBUG("PATH: %s", path);
  } else {
    LOG_ERROR("PATH not found");
  }

  char path_copy[strlen(path) + 1];
  strcpy(path_copy, path);  // 要複製一份，不然會改到原本的path
  char *s;
  s = strtok(path_copy, ":");
  while (s) {
    dir[ndir++] = s;  // token string pointers
    s = strtok(0, ":");
  }
  dir[ndir] = NULL;  // dir[n] = NULL pointer

  for (int i = 0; i < ndir; i++) {
    LOG_DEBUG("dir[%d] = %s", i, dir[i]);
  }
  return;
}

static void report_error_and_exit(const char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}

/*執行指令*/
void exec_command(char *command) {
  LOG_DEBUG("command: %s", command);

  tokenize(command);

  fflush(stdout);
  fflush(stdin);

  if (redirect_stdout) {
    close(1);
    open(redirect_stdout_filename, O_WRONLY | O_CREAT, 0644);
  } else if (redirect_stdin) {
    close(0);
    open(redirect_stdin_filename, O_RDONLY);
  }

  int r = execvp(args[0], args);
  if (r < 0) {
    report_error_and_exit("execvp");
  }

  exit(1);
}

/** move oldfd to newfd */
static void redirect(int oldfd, int newfd) {
  if (oldfd != newfd) {
    if (dup2(oldfd, newfd) != -1)
      Close(oldfd);
    else
      report_error_and_exit("dup2");
  }
}

/**
 * 依序執行commands[pos]中的指令，並將前一個指令的輸出導向到下一個指令的輸入
 */
static void exec_pipeline(int pos, int in_fd) {
  if (commands[pos + 1] == NULL) {  // 檢查是否是最後一個指令
    redirect(in_fd, STDIN_FILENO);  // read from in_fd, write to STDOUT
    exec_command(commands[pos]);
    fflush(stdout);
    fflush(stdin);
    Close(in_fd);
  } else {      // $ <in_fd cmds[pos] >fd[1] | <fd[0] cmds[pos+1] ...
    int fd[2];  // output pipe
    if (pipe(fd) == -1) report_error_and_exit("pipe");
    switch (fork()) {
      case -1:
        report_error_and_exit("fork");
        break;
      case 0:                            // child
        Close(fd[0]);                    // unused
        redirect(in_fd, STDIN_FILENO);   // read from in_fd
        redirect(fd[1], STDOUT_FILENO);  // write to fd[1]
        exec_command(commands[pos]);
        report_error_and_exit("execvp");
        break;
      default:         // parent
        Close(fd[1]);  // unused
        Close(in_fd);  // unused

        LOG_DEBUG("mysh %d forked a child sh %d", getpid(), pid);
        LOG_DEBUG("mysh %d wait for child sh %d to terminate", getpid(), pid);
        wait(&status);
        LOG_DEBUG("ZOMBIE child=%d exitStatus=%x", pid, status);
        LOG_DEBUG("mysh %d repeat loop", getpid());
        exec_pipeline(pos + 1, fd[0]);  // 繼續執行剩下的指令
    }
  }
}

void split_commands(char *line) {
  LOG_DEBUG("line: %s", line);
  ncommands = 0;
  char *s;
  char line_copy[64];
  strcpy(line_copy, line);
  s = strtok(line_copy, "|");
  while (s) {
    commands[ncommands] = (char *)malloc(strlen(s) + 1);
    strcpy(commands[ncommands], s);
    trim_white_space(commands[ncommands]);
    ncommands++;
    s = strtok(0, "|");
  }
  commands[ncommands] = NULL;
  for (int i = 0; i < ncommands; i++) {
    LOG_DEBUG("commands[%d] = %s", i, commands[i]);
  }
}

int main(int argc, char *argv[], char *env[]) {
  char line[128];

  stdin_copy = dup(STDIN_FILENO);
  stdout_copy = dup(STDOUT_FILENO);

  LOG_DEBUG("Starting program: %s", __FILENAME__);

  print_debug_info(argc, argv, env);

  while (1) {
    char cwd[128];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
      report_error_and_exit("getcwd");
    }

    LOG_DEBUG("mysh %d running", getpid());
    printf("%s > ",
           (__builtin_strrchr(cwd, '/') ? __builtin_strrchr(cwd, '/') + 1
                                        : cwd));  // print prompt

    fgets(line, 128, stdin);     // Accept ANY valid Linux command line
    line[strlen(line) - 1] = 0;  // kill \n at end

    if (line[0] == '\0')  // if line is NULL
      continue;
    fflush(stdin);
    split_commands(line);

    if (strstr(commands[0], "cd") != NULL) {
      tokenize(commands[0]);
      chdir(args[1]);
      continue;
    }
    if (strstr(commands[0], "exit") != NULL) exit(0);

    /*在POSIX.1-2001標準規定中，父行程可將SIGCHLD的處理常式設為SIG_IGN（亦為預設設定），
    或為SIGCHLD設定SA_NOCLDWAIT標記，以使Kernel可以自動回收已終止的子行程的資源。*/
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) report_error_and_exit("signal");

    pid = fork();
    switch (pid) {
      case -1:
        report_error_and_exit("fork");
        break;
      case 0:  // child
        exec_pipeline(0, STDIN_FILENO);
        report_error_and_exit("execvp");
        break;
      default:  // parent
        LOG_DEBUG("mysh %d forked a child sh %d", getpid(), pid);
        LOG_DEBUG("mysh %d wait for child sh %d to terminate", getpid(), pid);
        wait(&status);
        LOG_DEBUG("ZOMBIE child = %d exitStatus = %x", pid, status);
        LOG_DEBUG("mysh %d repeat loop", getpid());
    }
  }

  return 0;
}
