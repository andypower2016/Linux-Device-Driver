# Ubuntu-linux-headers-5.11.0-38-generic
A simple guide to build module in Linux 

Execute steps below on Command prompt
1) Make
2) sudo insmod hello.ko
3) sudo rmmod hello.ko
4) dmesg | tail  // https://man7.org/linux/man-pages/man1/dmesg.1.html

[ 3693.929757] Code: 00 c3 66 2e 0f 1f 84 00 00 00 00 00 90 f3 0f 1e fa 48 89 f8 48 89 f7 48 89 d6 48 89 ca 4d 89 c2 4d 89 c8 4c 8b 4c 24 08 0f 05 <48> 3d 01 f0 ff ff 73 01 c3 48 8b 0d c3 f5 0c 00 f7 d8 64 89 01 48
[ 3693.929758] RSP: 002b:00007ffc5d0db528 EFLAGS: 00000246 ORIG_RAX: 0000000000000139
[ 3693.929760] RAX: ffffffffffffffda RBX: 0000557b47d857a0 RCX: 00007ffa992f589d
[ 3693.929761] RDX: 0000000000000000 RSI: 0000557b460d9358 RDI: 0000000000000003
[ 3693.929762] RBP: 0000000000000000 R08: 0000000000000000 R09: 00007ffa993c9260
[ 3693.929763] R10: 0000000000000003 R11: 0000000000000246 R12: 0000557b460d9358
[ 3693.929764] R13: 0000000000000000 R14: 0000557b47d85760 R15: 0000000000000000
[ 3707.664683] exit world new
[ 3730.752790] hello world 
[ 3753.792758] exit world new
