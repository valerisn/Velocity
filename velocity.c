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
#include <signal.h>
#include <limits.h>

#define CGROUP_BASE "/sys/fs/cgroup/velocity"

struct container_args {
    char *rootfs;
    char **argv;
    long memory_limit;      // in bytes
    int  cpu_shares;
};

static void die(const char *msg)
{
    perror(msg);
    exit(1);
}

static void write_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    if (fd == -1)
        die("open");
    if (write(fd, value, strlen(value)) == -1)
        die("write");
    close(fd);
}

static void setup_user_namespace(pid_t pid)
{
    char path[256];

    snprintf(path, sizeof(path), "/proc/%d/setgroups", pid);
    write_file(path, "deny");

    snprintf(path, sizeof(path), "/proc/%d/uid_map", pid);
    char map[64];
    snprintf(map, sizeof(map), "0 %d 1\n", getuid());
    write_file(path, map);

    snprintf(path, sizeof(path), "/proc/%d/gid_map", pid);
    snprintf(map, sizeof(map), "0 %d 1\n", getgid());
    write_file(path, map);
}

static void setup_cgroups(pid_t pid, long memory_limit, int cpu_shares)
{
    char cgroup_path[256];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s_%d", CGROUP_BASE, pid);

    if (mkdir(cgroup_path, 0755) != 0 && errno != EEXIST)
        die("mkdir cgroup");

    char subpath[512];

    // Memory limit
    snprintf(subpath, sizeof(subpath), "%s/memory.max", cgroup_path);
    char mem_str[32];
    snprintf(mem_str, sizeof(mem_str), "%ld", memory_limit);
    write_file(subpath, mem_str);

    // CPU shares
    snprintf(subpath, sizeof(subpath), "%s/cpu.weight", cgroup_path);
    snprintf(mem_str, sizeof(mem_str), "%d", cpu_shares);
    write_file(subpath, mem_str);

    // Add process
    snprintf(subpath, sizeof(subpath), "%s/cgroup.procs", cgroup_path);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    write_file(subpath, pid_str);
}

static void mount_filesystems(const char *rootfs)
{
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0)
        die("mount private");

    if (chroot(rootfs) != 0 || chdir("/") != 0)
        die("chroot");

    // Essential mounts
    if (mount("proc", "/proc", "proc", 0, NULL) != 0)
        die("mount proc");

    if (mount("sysfs", "/sys", "sysfs", 0, NULL) != 0)
        die("mount sys");

    if (mount("tmpfs", "/tmp", "tmpfs", 0, NULL) != 0)
        die("mount tmpfs");

    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0)
        die("mount dev");

    // Basic devices
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    mknod("/dev/zero", S_IFCHR | 0666, makedev(1, 5));
    mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8));
    mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9));
}

static int container_init(void *arg)
{
    struct container_args *args = arg;

    if (sethostname("velocity", 8) != 0)
        die("sethostname");

    mount_filesystems(args->rootfs);

    printf("[Velocity] Container started. Executing: %s\n", args->argv[0]);

    if (execvp(args->argv[0], args->argv) == -1)
        die("execvp");

    return 1; // unreachable
}

int main(int argc, char **argv)
{
    if (argc < 4 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "Usage: %s run --root <rootfs> [options] <cmd> [args...]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  --memory <MB>     Memory limit in MB (default: 256)\n");
        fprintf(stderr, "  --cpu <shares>    CPU weight (default: 1024)\n");
        return 1;
    }

    struct container_args args = {
        .memory_limit = 256 * 1024 * 1024,
        .cpu_shares = 1024,
        .rootfs = NULL,
        .argv = NULL
    };

    int i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            args.rootfs = argv[++i];
        } else if (strcmp(argv[i], "--memory") == 0 && i + 1 < argc) {
            args.memory_limit = atol(argv[++i]) * 1024 * 1024;
        } else if (strcmp(argv[i], "--cpu") == 0 && i + 1 < argc) {
            args.cpu_shares = atoi(argv[++i]);
        } else {
            break;
        }
        i++;
    }

    if (!args.rootfs || i >= argc) {
        fprintf(stderr, "Error: --root is required and command must be provided\n");
        return 1;
    }

    args.argv = &argv[i];

    // Stack for child
    char *stack = malloc(1024 * 1024);
    if (!stack)
        die("malloc stack");

    // Clone with multiple namespaces
    int flags = CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWUSER | CLONE_NEWIPC;
    pid_t pid = clone(container_init, stack + 1024*1024, flags | SIGCHLD, &args);

    if (pid == -1)
        die("clone");

    setup_user_namespace(pid);
    setup_cgroups(pid, args.memory_limit, args.cpu_shares);

    int status;
    waitpid(pid, &status, 0);

    // Cleanup
    char cgroup_path[256];
    snprintf(cgroup_path, sizeof(cgroup_path), "%s_%d", CGROUP_BASE, pid);
    rmdir(cgroup_path);

    free(stack);
    printf("[Velocity] Container exited with status %d\n", WEXITSTATUS(status));

    return 0;
}
