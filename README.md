fuseloop
========

A small loopback mount utility using FUSE.

Mainly intended to allow creating of virtual disk images by giving specific offset/size access so that mke2fs can be run on less than the entire file.

Once a virtual disk image has been partitioned and had the filesystems created, [fuseext2](https://packages.debian.org/bullseye/misc/fuseext2) can be used to mount it.

Example:

    # Create a new file for our disk image (300M in size)
    truncate -s 300M mydisk.img
    
    # Use fdisk to create a 100M partition
    # If we use 64/32/512 for heads/sectors/sectorsize cylinders=$DISK_SIZE_IN_M
    /sbin/fdisk -c -u -C 300 -H 64 -S 32 -b 512 mydisk.img << EOF
    n
    p
    1
    
    +100M
    a
    w
    EOF
    
    # Use fdisk to give us the correct offset and size values to use with fuseloop
    line=`/sbin/fdisk -c -u -l -C 300 -H 64 -S 32 -b 512 mydisk.img | grep mydisk.img | grep Linux | sed 's/\*//'`
    set -- $line
    sectorsize=512
    offs=$(($2*$sectorsize))
    size=$((($3-$2)*$sectorsize))
      
    # Random name for partition dev
    part=part.$$
    touch "$part"
      
    # Use fuseloop to expose the partition as an individual file/device
    ./fuseloop -O "$offs" -S "$size" mydisk.img "$part"
      
    # Create the file system
    /sbin/mke2fs -F "$part"
      
    # If you want, you can install the SYSLinux MBR to get a bootable image (remember to mark partition 1 as 'active' too!)
    dd if=/usr/lib/syslinux/mbr/mbr.bin of=mydisk.img conv=notrunc
    
    # With the filesystem laid down on the partition, fuseext2 can be used to mount it
    mkdir mnt
    fuseext2 -o rw,force $part mnt
    # ... copy file to partition ...
  
    # Unmount the partition
    fusermount -u "$part"
    fusermount -u mnt
      
    # Clean up the partition file we mounted on
    rm -f "$part"
    rmdir mnt
    
  
