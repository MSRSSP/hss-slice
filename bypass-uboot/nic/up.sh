 /sbin/sysctl -w net.ipv4.ip_forward=1
 /sbin/iptables -t nat -A POSTROUTING -s 192.168.123.0/24 ! -d 192.168.123.0/24 -j MASQUERADE
ip link add br0 type bridge
ip addr add 192.168.123.1/24 dev br0
ip link set up dev br0
ip tuntap add vm0 mode tap
ip link set vm0 up
ip link set vm0 master br0
ip tuntap add vm2 mode tap 
ip link set vm2 up
ip link set vm2 master br0
sudo dnsmasq --interface=br0 --bind-interfaces --dhcp-range=192.168.123.0,192.168.123.255
