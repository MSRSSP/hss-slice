ifconfig vm0 down
ifconfig vm2 down
ifconfig br0 down
ifconfig wg0 down
brctl delbr br0
tunctl -d vm0
tunctl -d vm2
brctl show
ip link del dev wg0
pkill -9 dnsmasq
