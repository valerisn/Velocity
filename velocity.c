#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

struct container_args {
    char **argv;
    char *rootfs;
};

void write_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    write(fd, value, strlen(value));
    close(fd);
}

void setup_variables(int pid) {
    char path[1024];
    char map[1024];

    // CLONE_NEWUSER: User Namespace mapping
    sprintf(path, "/proc/%d/uid_map", pid);
    sprintf(map, "0 %d 1\n", getuid());
    write_file(path, map);

    sprintf(path, "/proc/%d/setgroups", pid);
    write_file(path, "deny");

    sprintf(path, "/proc/%d/gid_map", pid);
    sprintf(map, "0 %d 1\n", getgid());
    write_file(path, map);
}

void setup_cgroups(int pid) {
    char path[1024];
    sprintf(path, "/sys/fs/cgroup/velocity_%d", pid);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror("mkdir cgroup");
        return;
    }

    char mem_path[1024];
    sprintf(mem_path, "%s/memory.max", path);
    write_file(mem_path, "268435456");

    char proc_path[1024];
    sprintf(proc_path, "%s/cgroup.procs", path);
    char pid_str[16];
    sprintf(pid_str, "%d", pid);
    write_file(proc_path, pid_str);
}

int run_container(struct container_args *args) {
    if (sethostname("velocity", 8) != 0) {
        perror("sethostname");
        return 1;
    }

    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0) {
        perror("mount private");
        return 1;
    }

    if (chroot(args->rootfs) != 0 || chdir("/") != 0) {
        perror("chroot/chdir");
        return 1;
    }

    if (mount("proc", "/proc", "proc", 0, NULL) != 0) {
        perror("mount proc");
        return 1;
    }

    printf("Velocity: Executing %s\n", args->argv[0]);
    if (execvp(args->argv[0], args->argv) == -1) {
        perror("execvp");
        umount2("/proc", MNT_DETACH);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv) {
    if (argc < 4 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "Usage: sudo ./velocity run --root <path> <cmd> [args...]\n");
        return 1;
    }

    struct container_args args;
    args.rootfs = argv[3];
    args.argv = &argv[4];

    // CLONE_NEWUSER: User Namespace
    // CLONE_NEWNS: Mount Namespace
    // CLONE_NEWPID: PID Namespace
    // CLONE_NEWUTS: UTS (Hostname) Namespace
    int pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        if (unshare(CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWUSER) != 0) {
            perror("unshare");
            exit(1);
        }

        int inner_pid = fork();
        if (inner_pid == 0) {
            exit(run_container(&args));
        }
        
        int status;
        waitpid(inner_pid, &status, 0);
        exit(WEXITSTATUS(status));
    }

    setup_variables(pid);
    setup_cgroups(pid);

    waitpid(pid, NULL, 0);
    
    char cgroup_del[1024];
    sprintf(cgroup_del, "/sys/fs/cgroup/velocity_%d", pid);
    rmdir(cgroup_del);

    printf("Velocity: Container finished.\n");
    return 0;
}