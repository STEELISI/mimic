sudo echo "*         hard    nofile      500000
*         soft    nofile      500000
root      hard    nofile      500000
root      soft    nofile      500000" >> /etc/security/limits.conf

sudo echo "session required pam_limits.so" >> /etc/pam.d/common-session

sudo echo "fs.file-max = 2097152" >> /etc/sysctl.conf

sudo sysctl -p

sysctl -w net.core.rmem_max=8388608 
sysctl -w net.core.wmem_max=8388608 
sysctl -w net.core.rmem_default=65536 
sysctl -w net.core.wmem_default=65536 
sysctl -w net.ipv4.tcp_mem='8388608 8388608 8388608' 
sysctl -w net.ipv4.tcp_rmem='4096 87380 8388608' 
sysctl -w net.ipv4.tcp_wmem='4096 65536 8388608' 
sysctl -w net.ipv4.route.flush=1

