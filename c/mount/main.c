#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>

#define STACK_SIZE (1024 * 1024)  // Stack size for cloned child
#define CLONE_FLAGS (CLONE_NEWUTS | CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWPID)  // Clone flags for new namespaces

static char child_stack[STACK_SIZE];

void create_txt_file() {
    int fd = open("/tmp/pratik.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (write(fd, "pratik", 6) != 6) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void exec_container_shell() {
    printf("Ready to exec container shell...\n");

    // Set the hostname in the new UTS namespace
    if (sethostname("newhost", strlen("newhost")) != 0) {
        perror("sethostname");
        exit(EXIT_FAILURE);
    }

    printf("Changing to /tmp directory...\n");
    if (chdir("/tmp") != 0) {
        perror("chdir");
        exit(EXIT_FAILURE);
    }

    printf("Mounting / as private...\n");
    if (mount("", "/", "", MS_PRIVATE | MS_REC, "") != 0) {
        perror("mount / as private");
        exit(EXIT_FAILURE);
    }

    printf("Binding rootfs/ to rootfs/ ...\n");
    if (mount("rootfs/", "rootfs/", "", MS_BIND | MS_REC, "") != 0) {
        perror("mount rootfs/");
        exit(EXIT_FAILURE);
    }

    printf("Pivot new root at rootfs/ ...\n");
    if (mkdir("rootfs/.old_root", 0755) != 0 && errno != EEXIST) {
        perror("mkdir rootfs/.old_root");
        exit(EXIT_FAILURE);
    }
    if (syscall(SYS_pivot_root, "rootfs/", "rootfs/.old_root") != 0) {  // Using SYS_pivot_root
        perror("pivot_root");
        exit(EXIT_FAILURE);
    }

    printf("Changing to / directory ...\n");
    if (chdir("/") != 0) {
        perror("chdir /");
        exit(EXIT_FAILURE);
    }

    printf("Mounting /tmp as tmpfs ...\n");
    if (mount("tmpfs", "/tmp", "tmpfs", MS_NODEV, "") != 0) {
        perror("mount tmpfs");
        exit(EXIT_FAILURE);
    }

    printf("Mounting /proc filesystem ...\n");
    if (mount("proc", "/proc", "proc", MS_NODEV, "") != 0) {
        perror("mount proc");
        exit(EXIT_FAILURE);
    }

    create_txt_file();

    printf("Mounting /.old_root as private ...\n");
    if (mount("", "/.old_root", "", MS_PRIVATE | MS_REC, "") != 0) {
        perror("mount /.old_root as private");
        exit(EXIT_FAILURE);
    }

    printf("Unmount parent rootfs from /.old_root ...\n");
    if (umount2("/.old_root", MNT_DETACH) != 0) {
        perror("umount2 /.old_root");
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
