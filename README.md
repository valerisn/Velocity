# Velocity 🚀

Velocity is a minimalistic Linux container runtime written in C. 

This project is a streamlined, single-file remake of my previous work [runtime](https://github.com/fallingintoyourarms/runtime). It was built to explore Linux primitives like namespaces, cgroups, and filesystem isolation in a clean, readable format.

### ⚠️ Disclaimer
**Velocity was made for fun and educational purposes.** It is not intended for production use or security-critical environments. It is a "toy" runtime meant for learning how containers work under the hood.

### Features
* **Namespaces:** Process, Mount, and UTS isolation.
* **Filesystem:** Chroot-based rootfs isolation.
* **Clean CLI:** Simple `run` command structure.

### Quick Start
1. **Build:**
   ```bash
   gcc velocity.c -o velocity