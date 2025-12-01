#!/bin/bash
# Script to write xv6.img to a USB drive for booting on real hardware
#
# Usage: sudo ./write-usb.sh /dev/sdX
# where /dev/sdX is your USB drive (e.g., /dev/sdb)
#
# WARNING: This will DESTROY all data on the target device!

set -e

if [ "$#" -ne 1 ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo "Example: sudo $0 /dev/sdb"
    echo ""
    echo "WARNING: This will erase all data on the target device!"
    exit 1
fi

if [ "$EUID" -ne 0 ]; then
    echo "ERROR: This script must be run as root (use sudo)"
    exit 1
fi

DEVICE="$1"
IMAGE="xv6.img"

# Verify the device exists
if [ ! -b "$DEVICE" ]; then
    echo "ERROR: $DEVICE is not a block device"
    exit 1
fi

# Verify the image exists
if [ ! -f "$IMAGE" ]; then
    echo "ERROR: $IMAGE not found. Run 'make' first."
    exit 1
fi

# Safety check: make sure it's not a system disk
if echo "$DEVICE" | grep -q "sda"; then
    read -p "WARNING: $DEVICE might be your system disk! Continue? (yes/no): " confirm
    if [ "$confirm" != "yes" ]; then
        echo "Aborted."
        exit 1
    fi
fi

echo "========================================="
echo "Writing xv6.img to $DEVICE"
echo "========================================="
echo ""
echo "Target device: $DEVICE"
echo "Image file: $IMAGE"
echo "Image size: $(du -h $IMAGE | cut -f1)"
echo ""
read -p "This will DESTROY all data on $DEVICE. Continue? (yes/no): " confirm

if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# Unmount any mounted partitions
echo "Unmounting any mounted partitions..."
umount ${DEVICE}* 2>/dev/null || true

# Write the image
echo "Writing image to $DEVICE..."
dd if="$IMAGE" of="$DEVICE" bs=512 conv=sync,noerror status=progress

# Sync to ensure all data is written
echo "Syncing..."
sync

echo ""
echo "========================================="
echo "SUCCESS!"
echo "========================================="
echo ""
echo "The xv6 image has been written to $DEVICE"
echo "You can now safely remove the USB drive and boot from it."
echo ""
echo "BIOS Boot Instructions:"
echo "1. Insert the USB drive into the target computer"
echo "2. Enter BIOS/UEFI settings (usually F2, F12, Del, or Esc during boot)"
echo "3. Disable Secure Boot if enabled"
echo "4. Set boot mode to Legacy/CSM (not UEFI)"
echo "5. Select the USB drive as the boot device"
echo "6. Save and exit"
echo ""
