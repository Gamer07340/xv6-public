#!/bin/bash
# Verify that the xv6.img has a proper boot sector for USB boot

echo "========================================="
echo "xv6 Boot Sector Verification"
echo "========================================="
echo ""

if [ ! -f "xv6.img" ]; then
    echo "ERROR: xv6.img not found. Run 'make' first."
    exit 1
fi

echo "Checking boot sector structure..."
echo ""

# Extract first 512 bytes
BOOT_SECTOR=$(dd if=xv6.img bs=512 count=1 2>/dev/null | hexdump -C)

# Check for jump instruction (EB 3C 90)
if echo "$BOOT_SECTOR" | head -1 | grep -q "eb 3c 90"; then
    echo "✓ Jump instruction found (EB 3C 90)"
else
    echo "✗ Jump instruction NOT found"
    exit 1
fi

# Check for BPB OEM name
if echo "$BOOT_SECTOR" | head -1 | grep -q "MSDOS5.0"; then
    echo "✓ BPB OEM name found (MSDOS5.0)"
else
    echo "✗ BPB OEM name NOT found"
    exit 1
fi

# Check for volume label
if echo "$BOOT_SECTOR" | grep -q "xv6 O"; then
    echo "✓ Volume label found (xv6 OS)"
else
    echo "✗ Volume label NOT found"
    exit 1
fi

# Check for boot signature (55 AA at offset 0x1FE)
SIGNATURE=$(dd if=xv6.img bs=1 skip=510 count=2 2>/dev/null | hexdump -e '2/1 "%02x"')
if [ "$SIGNATURE" = "55aa" ]; then
    echo "✓ Boot signature found (55 AA)"
else
    echo "✗ Boot signature NOT found (got: $SIGNATURE)"
    exit 1
fi

# Check boot sector size
BOOT_SIZE=$(stat -c%s bootblock 2>/dev/null)
if [ "$BOOT_SIZE" = "512" ]; then
    echo "✓ Boot sector is exactly 512 bytes"
else
    echo "✗ Boot sector is $BOOT_SIZE bytes (should be 512)"
    exit 1
fi

echo ""
echo "========================================="
echo "✓ All checks passed!"
echo "========================================="
echo ""
echo "The boot sector is properly formatted for USB boot."
echo "You can now write this image to a USB drive using:"
echo "  sudo ./write-usb.sh /dev/sdX"
echo ""
echo "Boot sector structure:"
dd if=xv6.img bs=512 count=1 2>/dev/null | hexdump -C | head -3
echo "..."
dd if=xv6.img bs=512 count=1 2>/dev/null | hexdump -C | tail -3
