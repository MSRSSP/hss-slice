make clean
make BOARD=slice ENV=QEMU FW_PIC=y VERBOSE=1 genconfig
make BOARD=slice ENV=QEMU FW_PIC=y VERBOSE=1 -j$(nproc)
