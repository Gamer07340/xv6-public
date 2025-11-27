#!/bin/bash
# Script to write xv6 to a USB drive with proper partitioning
# Usage: sudo ./write-usb.sh /dev/sdX (where X is your USB drive letter)

if [ "$#" -ne 1 ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo "Example: sudo $0 /dev/sdb"
    echo ""
    echo "WARNING: This will DESTROY all data on the specified device!"
    echo ""
    echo "Available devices:"
    lsblk -d -o NAME,SIZE,TYPE,TRAN | grep -E "disk.*usb"
    exit 1
fi

DEVICE=$1

# Safety check
if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root (use sudo)"
    exit 1
fi

echo "WARNING: This will erase ALL data on $DEVICE"
echo "Press Ctrl+C to cancel, or Enter to continue..."
read

# Unmount any mounted partitions
umount ${DEVICE}* 2>/dev/null || true

# Write the raw image directly
echo "Writing xv6.img to $DEVICE..."
dd if=xv6.img of=$DEVICE bs=4M status=progress conv=fsync

# Sync to ensure all data is written
sync

echo ""
echo "Done! You can now boot from $DEVICE"
echo ""
echo "If your BIOS still doesn't recognize it, try these options:"
echo "1. Enable 'Legacy Boot' or 'CSM' in BIOS"
echo "2. Disable 'Secure Boot'"
echo "3. Set boot mode to 'Legacy' instead of 'UEFI'"
