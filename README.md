fuseloop
========

A small loopback mount utility using FUSE.

Mainly intended to allow creating of virtual disk images by giving specific offset/size access so that mke2fs can be run on less than the entire file.

Once a virtual disk image has been partitioned and had the filesystems created, "mountlo" can be used to mount them.

Example:

    # Create a new file for our disk image (300M in size)
    truncate -s 300M mydisk.img
    
    # Use fdisk to create a 100M partition
    # If we use 64/32/512 for heads/sectors/sectorsize cylinders=$DISK_SIZE_IN_M
    fdisk -c -u -C 300 -H 64 -S 32 -b 512 mydisk.img << EOF
    n
    p
    1
    
    +100M
    w
    EOF
    sectorsize=512
    
    # Use fdisk to give us the correct offset and size values to use with fuseloop
    fdisk -c -u -l -C 300 -H 64 -S 32 -b 512 mydisk.img | grep mydisk.img | grep Linux | while read line
    do
      set -- $line
      offs=$(($2*$sectorsize))
      size=$((($3-$2)*$sectorsize))
      
      # Random name for partition dev
      part=part.$$
      touch "$part"
      
      # Use fuseloop to expose the partition as an individual file/device
      fuseloop -O "$offs" -S "$size" mydisk.img "$part"
      
      # Create the file system
      mke2fs -F "$part"
      
      # Unmount the partition
      fusermount -u "$part"
      
      # Clean up the partition file we mounted on
      rm -f "$part"
    done
    
    # If you want, you can install the SYSLinux MBR to get a bootable image (remember to mark partition 1 as 'active' too!)
    dd if=/usr/lib/syslinux/mbr.bin of=mydisk.img conv=notrunc
    
    # With the filesystem laid down on the partition, mountlo can be used to mount it
    mountlo -p1 -w mydisk.img /mnt
  
  
