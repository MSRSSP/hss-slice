make clean
mkdir -p Default-qemu
make BOARD=slice ENV=QEMU FW_PIC=y VERBOSE=1 genconfig
make BOARD=slice ENV=QEMU FW_PIC=y VERBOSE=1 -j$(nproc)
