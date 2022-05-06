make clean
make BOARD=slice ENV=REAL FW_PIC=y VERBOSE=1 genconfig
make BOARD=slice ENV=REAL FW_PIC=y VERBOSE=1 -j$(nproc)
