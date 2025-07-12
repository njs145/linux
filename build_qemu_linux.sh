#!/bin/bash

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
echo "The build is running in the background. To check on it, run: tmux attach -t linux_kernel-build"
echo "To detach and return to your shell, press Ctrl + b, then d."
tmux new-session -d -s linux_kernel-build -- \
"ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make -j$(nproc)"
