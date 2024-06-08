#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#define STACK_SIZE (1024 * 1024)  // Stack size for cloned child
#define CLONE_FLAGS (CLONE_NEWUTS | CLONE_NEWUSER)  // Clone flags for new UTS and user namespace

static char child_stack[STACK_SIZE];

void exec_container_shell() {
    printf("Ready to exec container shell...\n");

    // Set the hostname in the new UTS namespace
    if (sethostname("newhost", strlen("newhost")) != 0) {
        perror("sethostname");
        exit(EXIT_FAILURE);
    }

    // Prepare the environment variables
    char *envp[] = { "PS1=-> ", NULL };

    // Execute a new shell
    char *args[] = { "/bin/sh", NULL };
    if (execve("/bin/sh", args, envp) == -1) {
        perror("execve");
        exit(EXIT_FAILURE);
    }
}

int child_func(void *arg) {
    exec_container_shell();
    return 0;  
}

void setup_uid_gid_mapping(pid_t pid) {
    char path[256];
    FILE *f;

    snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
    f = fopen(path, "w");
    if (f == NULL) {
        perror("fopen uid_map");
        exit(EXIT_FAILURE);
    }
    fprintf(f, "0 %d 1\n", getuid());
    fclose(f);

    snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
    f = fopen(path, "w");
    if (f == NULL) {
        perror("fopen setgroups");
        exit(EXIT_FAILURE);
    }
    fprintf(f, "deny\n");
    fclose(f);

    snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
    f = fopen(path, "w");
    if (f == NULL) {
        perror("fopen gid_map");
        exit(EXIT_FAILURE);
    }
    fprintf(f, "0 %d 1\n", getgid());
    fclose(f);
}

int main(int argc, char *argv[]) {
    printf("Starting process %s with args: ", argv[0]);
    for (int i = 0; i < argc; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    const char *clone_arg = "CLONE";

    if (argc > 1 && strcmp(argv[1], clone_arg) == 0) {
        exec_container_shell();
        exit(EXIT_SUCCESS);
    }

    printf("Ready to run command ...\n");

    pid_t pid = clone(child_func, child_stack + STACK_SIZE, CLONE_FLAGS | SIGCHLD, NULL);
    if (pid == -1) {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    // Set up UID and GID mapping
    setup_uid_gid_mapping(pid);

    // Wait for the cloned child to exit
    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }

    return 0;
}
