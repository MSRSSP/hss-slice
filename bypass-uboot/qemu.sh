QEMU=${TOP}/qemu/build/qemu-system-riscv64
HSS=${TOP}/slice-hss/Default-qemu/hss-envm-wrapper.bin
SD=$1 #build/slice-qemu-sd.img
DEBUG_OPT="-d opensbi -D log.txt -s"
OPT="-monitor telnet:127.0.0.1:4322,server,nowait"
${QEMU} ${DEBUG_OPT} -M microchip-icicle-kit -smp 5 -m 4G \
	-bios ${HSS}	-sd ${SD} ${OPT} \
	-display none -serial stdio \
	-nic tap,ifname=vm0,model=cadence_gem,script=no \
	-nic tap,ifname=vm2,model=cadence_gem,script=no \
	-serial telnet:localhost:5431,server,nowait \
	-serial telnet:localhost:5432,server,nowait \
	-serial telnet:localhost:5433,server,nowait
