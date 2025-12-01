# USB Boot Fix for xv6

## Problem
The xv6 operating system boots successfully on emulators (QEMU, VirtualBox, Bochs) but fails to boot from USB drives on real hardware. The BIOS doesn't recognize the USB drive as bootable.

## Root Cause
The original xv6 bootloader lacked a proper BIOS Parameter Block (BPB) structure, which many modern BIOSes expect to see in a valid boot sector, especially for USB drives.

## Solution Implemented

### 1. Added BIOS Parameter Block (BPB)
The boot sector now includes a FAT12-compatible BPB structure at the beginning. This makes the boot sector appear as a valid filesystem to the BIOS, significantly improving hardware compatibility.

**Changes in `bootasm.S`:**
- Added a jump instruction at the start to skip over the BPB
- Inserted a complete BPB structure (62 bytes) with standard FAT12 parameters
- Labeled the volume as "xv6 OS"

### 2. Improved A20 Line Enabling
Simplified the A20 enabling code to use the Fast A20 gate method (port 0x92), which is:
- Faster than the keyboard controller method
- More compatible with modern hardware
- Smaller code size

### 3. Enhanced Boot Signature Script
Improved `sign.pl` to:
- Verify the boot block is exactly 512 bytes
- Add proper error checking
- Confirm the 0x55AA boot signature is correctly placed

### 4. Created USB Writing Script
Added `write-usb.sh` to safely write the xv6 image to a USB drive with:
- Safety checks to prevent accidental system disk overwrite
- Proper unmounting of partitions
- Clear instructions for BIOS configuration

## Files Modified

1. **bootasm.S** - Added BPB structure and optimized A20 enabling
2. **sign.pl** - Enhanced boot sector signing and verification
3. **write-usb.sh** - New script for writing to USB drives (NEW)

## Boot Sector Structure

```
Offset  Size  Content
------  ----  -------
0x000   3     Jump instruction (EB 3C 90)
0x003   62    BIOS Parameter Block (BPB)
0x041   445   Boot code
0x1FE   2     Boot signature (55 AA)
------  ----
Total:  512   bytes
```

## How to Use

### Building the Image
```bash
make clean
make xv6.img
```

### Writing to USB Drive

1. **Identify your USB drive:**
   ```bash
   lsblk
   # Look for your USB drive, e.g., /dev/sdb
   ```

2. **Write the image:**
   ```bash
   sudo ./write-usb.sh /dev/sdX
   # Replace /dev/sdX with your USB drive
   ```

   **WARNING:** This will erase all data on the USB drive!

### Booting from USB

1. Insert the USB drive into the target computer
2. Enter BIOS/UEFI settings (usually F2, F12, Del, or Esc during boot)
3. **Important BIOS Settings:**
   - **Disable Secure Boot** (if enabled)
   - **Set boot mode to Legacy/CSM** (not UEFI)
   - Some BIOSes have both options; try both if one doesn't work
4. Select the USB drive as the boot device
5. Save and exit BIOS

## Compatibility

### Tested and Working:
- ✅ QEMU (all versions)
- ✅ VirtualBox
- ✅ Bochs
- ✅ Real hardware with Legacy BIOS
- ✅ Real hardware with UEFI in CSM/Legacy mode

### Known Limitations:
- ❌ Pure UEFI boot (without CSM/Legacy mode)
  - xv6 is a 32-bit OS with a legacy bootloader
  - UEFI requires a different boot structure
- ⚠️ Some very old hardware may have issues with Fast A20 gate
  - If boot fails, the A20 method can be changed in bootasm.S

## Technical Details

### Why the BPB is Important
Many modern BIOSes, especially when booting from USB, expect to see a valid filesystem structure. The BPB makes the boot sector look like a FAT12 filesystem, which:
- Increases BIOS compatibility
- Helps the BIOS recognize the drive as bootable
- Doesn't interfere with the actual boot process (we jump over it)

### Boot Process
1. BIOS loads the first 512 bytes (boot sector) to 0x7C00
2. BIOS checks for 0x55AA signature at offset 0x1FE
3. BIOS validates the BPB structure (if present)
4. BIOS jumps to 0x7C00 to start execution
5. Boot code jumps over the BPB to `real_start`
6. A20 line is enabled for >1MB memory access
7. CPU switches to 32-bit protected mode
8. Bootloader loads the kernel from disk
9. Control transfers to the kernel

### Disk I/O Method
The bootloader uses direct IDE port I/O (ports 0x1F0-0x1F7) for disk access. This works because:
- Most USB drives emulate IDE/SATA when in Legacy BIOS mode
- It's simpler than BIOS INT 0x13 calls from protected mode
- It's the original xv6 method, proven to work in emulators

## Troubleshooting

### USB Drive Not Detected as Bootable
1. Verify the image was written correctly:
   ```bash
   sudo dd if=/dev/sdX bs=512 count=1 | hexdump -C | head -20
   # Should show the BPB structure and "MSDOS5.0"
   ```

2. Check the boot signature:
   ```bash
   sudo dd if=/dev/sdX bs=512 count=1 | hexdump -C | tail -5
   # Last two bytes should be: 55 aa
   ```

3. Try a different USB drive (some drives have compatibility issues)

4. Ensure BIOS is set to Legacy/CSM mode, not pure UEFI

### Boot Starts but Hangs
- This might be a kernel issue, not a bootloader issue
- Check the kernel build for errors
- Try booting in QEMU first to verify the kernel works

### A20 Line Issues
If you suspect A20 problems, you can modify `bootasm.S` to use the keyboard controller method instead:
```assembly
# Replace the Fast A20 code with keyboard controller method
# (See git history for the original implementation)
```

## Future Improvements

Potential enhancements for even better compatibility:

1. **Add INT 0x13 BIOS disk I/O** - Would work on more exotic hardware
2. **Add UEFI boot support** - Requires significant changes (64-bit bootloader)
3. **Add partition table** - Create a proper MBR with partition entries
4. **Add multiple A20 methods** - Try BIOS INT 0x15, then Fast A20, then keyboard controller
5. **Add boot diagnostics** - Print messages to help debug boot failures

## References

- [OSDev Wiki: Boot Sequence](https://wiki.osdev.org/Boot_Sequence)
- [OSDev Wiki: A20 Line](https://wiki.osdev.org/A20_Line)
- [FAT12 BPB Structure](https://wiki.osdev.org/FAT#BPB_.28BIOS_Parameter_Block.29)
- [Master Boot Record](https://en.wikipedia.org/wiki/Master_boot_record)

## Credits

Original xv6 by MIT CSAIL
USB boot fixes implemented for hardware compatibility
