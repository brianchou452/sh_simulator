/***** LAB2 base code *****/
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

#define Close(FD)                                                       \
  do {                                                                  \
    int Close_fd = (FD);                                                \
    if (close(Close_fd) == -1) {                                        \
      perror("close");                                                  \
      fprintf(stderr, "%s:%d: close(" #FD ") %d\n", __FILE__, __LINE__, \
              Close_fd);                                                \
    }                                                                   \
  } while (0)

char gpath[128];  // hold token strings
char *args[64];   // token string pointers
int n;            // number of token strings

char dpath[128];  // hold dir strings in PATH
char *dir[64];    // dir string pointers
int ndir = 0;     // number of dirs

int pid, status;

bool redirect_stdout = false;
char *redirect_stdout_filename;

char *commands[64];
int ncommands = 0;

int stdin_copy = 0;
int stdout_copy = 0;
int fd1[2];
int fd2[2];

int tokenize(char *pathname) {
  char *s;
  redirect_stdout = false;  // reset
  strcpy(gpath, pathname);  // copy into global gpath[]
  s = strtok(gpath, " ");
  n = 0;
  while (s) {
    args[n] = malloc(strlen(s) + 1);  // token string pointers
    if (strcmp(s, ">") == 0) {
      redirect_stdout = true;
      s = strtok(0, " ");
      redirect_stdout_filename = malloc(strlen(s) + 1);
      strcpy(redirect_stdout_filename, s);
      break;
    }
    strcpy(args[n], s);  // copy string at s to args[]
    n++;
    s = strtok(0, " ");
  }
  args[n] = NULL;  // name[n] = NULL pointer

  for (char **p = args; *p != NULL; p++) {
    LOG_DEBUG("args[%ld] = %s", p - args, *p);
  }
  LOG_DEBUG("args[%d] = %s", n, args[n]);
  return 0;
}

// print argc, argv and env
void print_debug_info(int argc, char *argv[], char *env[]) {
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

void exec_command(char *command) {
  LOG_DEBUG("command: %s", command);
  char *cmd;

  tokenize(command);  // divide line into token strings

  cmd = args[0];

  if (strcmp(cmd, "cd") == 0) {
    chdir(args[1]);
    return;
  }
  if (strcmp(cmd, "exit") == 0) exit(0);

  fflush(stdout);
  fflush(stdin);

  if (redirect_stdout) {
    close(1);
    open(redirect_stdout_filename, O_WRONLY | O_CREAT, 0644);
  }

  int r = execvp(cmd, args);
  if (r < 0) {
    perror("execlp failed");
  }

  exit(1);
}
static void report_error_and_exit(const char *msg) {
  perror(msg);
  //(child ? _exit : exit)(EXIT_FAILURE);
  exit(EXIT_FAILURE);
}

/** move oldfd to newfd */
static void redirect(int oldfd, int newfd) {
  if (oldfd != newfd) {
    if (dup2(oldfd, newfd) != -1)
      Close(oldfd); /* successfully redirected */
    else
      report_error_and_exit("dup2");
  }
}

/** execute `cmds[pos]` command and call itself for the rest of the commands.
    `cmds[]` is NULL-terminate array
    `exec_pipeline()` never returns.
*/
static void exec_pipeline(size_t pos, int in_fd) {
  if (commands[pos + 1] == NULL) { /* last command */
    redirect(in_fd, STDIN_FILENO); /* read from in_fd, write to STDOUT */
    exec_command(commands[pos]);
    fflush(stdout);
    fflush(stdin);
    Close(in_fd);

    // execvp(cmds[pos][0], cmds[pos]);
    // report_error_and_exit("execvp last");
  } else {     /* $ <in_fd cmds[pos] >fd[1] | <fd[0] cmds[pos+1] ... */
    int fd[2]; /* output pipe */
    if (pipe(fd) == -1) report_error_and_exit("pipe");
    switch (fork()) {
      case -1:
        report_error_and_exit("fork");
        break;
      case 0: /* child: execute current command `cmds[pos]` */
        // child = 1;
        Close(fd[0]);                   /* unused */
        redirect(in_fd, STDIN_FILENO);  /* read from in_fd */
        redirect(fd[1], STDOUT_FILENO); /* write to fd[1] */
        // execvp(cmds[pos][0], cmds[pos]);
        exec_command(commands[pos]);
        report_error_and_exit("execvp");
        break;
      default:        /* parent: execute the rest of the commands */
        Close(fd[1]); /* unused */
        Close(in_fd); /* unused */

        LOG_DEBUG("mysh %d forked a child sh %d", getpid(), pid);
        LOG_DEBUG("mysh %d wait for child sh %d to terminate", getpid(), pid);
        wait(&status);
        LOG_DEBUG("ZOMBIE child=%d exitStatus=%x", pid, status);
        LOG_DEBUG("mysh %d repeat loop", getpid());
        exec_pipeline(pos + 1, fd[0]); /* execute the rest */
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
    strcpy(commands[ncommands++], s);
    s = strtok(0, "|");
  }
  commands[ncommands] = NULL;
  for (int i = 0; i < ncommands; i++) {
    LOG_DEBUG("commands[%d] = %s", i, commands[i]);
  }
}

int main(int argc, char *argv[], char *env[]) {
  char *cmd;
  char line[128];

  stdin_copy = dup(STDIN_FILENO);
  stdout_copy = dup(STDOUT_FILENO);

  LOG_DEBUG("Starting program: %s", __FILENAME__);

  // print_debug_info(argc, argv, env);

  while (1) {
    LOG_DEBUG("mysh %d running", getpid());
    printf("enter a command line : ");  // cat file1 file2
                                        // ls -l -a -f
                                        // cat file | grep print
                                        // ANY valid Linux command line
    fgets(line, 128, stdin);
    line[strlen(line) - 1] = 0;  // kill \n at end

    if (line[0] == '\0')  // if (strcmp(line, "")==0) // if line is NULL
      continue;
    fflush(stdin);
    split_commands(line);
    // split_commands("cat animal.txt|more");
    //  recursive_pipe(ncommands - 1);
    if (signal(SIGCHLD, SIG_IGN) == SIG_ERR) /* POSIX.1-2001 reap children */
      report_error_and_exit("signal");
    // exec_pipeline(0, STDIN_FILENO);

    switch (fork()) {
      case -1:
        report_error_and_exit("fork");
        break;
      case 0: /* child: execute current command `cmds[pos]` */
        exec_pipeline(0, STDIN_FILENO);
        report_error_and_exit("execvp");
        break;
      default: /* parent: execute the rest of the commands */
        LOG_DEBUG("mysh %d forked a child sh %d", getpid(), pid);
        LOG_DEBUG("mysh %d wait for child sh %d to terminate", getpid(), pid);
        wait(&status);
        LOG_DEBUG("ZOMBIE child=%d exitStatus=%x", pid, status);
        LOG_DEBUG("mysh %d repeat loop", getpid());
    }
  }

  return 0;
}

/********************* YOU DO ***********************
1. I/O redirections:

Example: line = arg0 arg1 ... > argn-1

  check each arg[i]:
  if arg[i] = ">" {
     arg[i] = 0; // null terminated arg[ ] array
     // do output redirection to arg[i+1] as in Page 131 of BOOK
  }
  Then execve() to change image


2. Pipes:

Single pipe   : cmd1 | cmd2 :  Chapter 3.10.3, 3.11.2

Multiple pipes: Chapter 3.11.2
****************************************************/
