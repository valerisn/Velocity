<div align="center">

# 🚀 Velocity

**A minimalistic Linux container runtime written in pure C.**

A clean, single-file exploration of the Linux kernel's container primitives.

![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

</div>

## About

**Velocity** is a streamlined, educational container runtime. It's a cleaner rewrite of my previous project [runtime](https://github.com/fallingintoyourarms/runtime).

Built to deeply understand how real container engines work under the hood.

> **For learning. For fun. Not for production.**

---

## Architecture

```
                     +---------------------+
                     |   Host System       |
                     |  (User + Privileges)|
                     +----------+----------+
                                |
                                | sudo ./velocity run ...
                                v
                     +---------------------+
                     |   Velocity CLI      |
                     +----------+----------+
                                |
                                v
                     +---------------------+
                     |      clone()        |
                     |  with namespaces    |
                     +----------+----------+
                                |
          +---------------------+---------------------+
          |                                           |
          v                                           v
+-------------------+                       +-------------------+
| User Namespace    |                       | Mount Namespace   |
| (UID/GID mapping) |                       | (chroot + mounts) |
+-------------------+                       +-------------------+
          |                                           |
          +---------------------+---------------------+
                                |
                                v
                     +---------------------+
                     |   Other Namespaces  |
                     | - PID               |
                     | - UTS (hostname)    |
                     | - IPC               |
                     +----------+----------+
                                |
                                v
                     +---------------------+
                     |   Cgroup v2         |
                     | - Memory Limit      |
                     | - CPU Weight        |
                     +----------+----------+
                                |
                                v
                     +---------------------+
                     |   Container Init    |
                     | - sethostname       |
                     | - mount proc/sys    |
                     | - execvp(command)   |
                     +---------------------+
```

---

## Features

- **Full Namespaces**: Mount, PID, UTS, User, IPC
- **Resource Control** via cgroup v2 (memory + CPU)
- **Filesystem Isolation**: Private mounts + chroot
- **Essential virtual filesystems** mounted (`/proc`, `/sys`, `/dev`, `/tmp`)
- Clean, single-file C implementation (~300 LOC)

---

## Quick Start

### 1. Build

```bash
gcc -O2 -static velocity.c -o velocity
```

### 2. Run

```bash
sudo ./velocity run --root ./rootfs alpine sh
```

With limits:

```bash
sudo ./velocity run --root ./rootfs --memory 512 --cpu 512 ubuntu /bin/bash
```

---

## Usage

```bash
sudo ./velocity run --root <rootfs> [options] <command> [args...]
```

**Options:**

| Option           | Description                    | Default |
|------------------|--------------------------------|---------|
| `--root`         | Root filesystem path           | Required|
| `--memory <MB>`  | Memory limit (MB)              | 256     |
| `--cpu <weight>` | CPU weight                     | 1024    |

---

## Example Rootfs (Alpine)

```bash
mkdir rootfs && cd rootfs
curl -L https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.3-x86_64.tar.gz | tar -xz
cd ..
sudo ./velocity run --root rootfs sh
```

---

## Roadmap

- [ ] Network namespace + veth
- [ ] Seccomp filtering
- [ ] Bind mounts / volumes
- [ ] OCI runtime compatibility
- [ ] Interactive TTY support

---

## Disclaimer

**Velocity is a toy project** for educational purposes only. It is **not secure** for running untrusted workloads.

---

## License

MIT
