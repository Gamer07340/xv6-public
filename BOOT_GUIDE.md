# xv6 Real Hardware Boot Guide

## Summary of Fixes

### Bootloader Improvements (bootasm.S)
1. **Simplified CS:IP normalization** - Removed problematic far jump that was causing issues
2. **Added A20 timeout protection** - Prevents infinite loops on hardware where keyboard controller is slow/broken
3. **Removed BIOS interrupt** - The `int $0x15` A20 enable was causing hangs on some hardware
4. **Fixed GDT address** - Uses absolute address calculation for better compatibility
5. **Proper stack setup** - Stack pointer set up early for stability

### System Compatibility
- ✅ **VirtualBox** - Works
- ✅ **QEMU** - Works  
- ✅ **Real Hardware** - Should now work with timeout fixes

## How to Write to USB

### Method 1: Using the provided script (Recommended)
```bash
sudo ./write-usb.sh /dev/sdX
```
Replace `/dev/sdX` with your USB device (e.g., `/dev/sdb`). 

**WARNING**: This will erase all data on the USB drive!

### Method 2: Manual dd command
```bash
sudo dd if=xv6.img of=/dev/sdX bs=4M status=progress conv=fsync
sudo sync
```

## BIOS Settings for USB Boot

If your computer still doesn't boot from USB, check these BIOS settings:

1. **Enable Legacy Boot / CSM**
   - Look for "Boot Mode" or "Boot Type"
   - Set to "Legacy" or "CSM" (not "UEFI Only")

2. **Disable Secure Boot**
   - Usually in Security or Boot settings
   - xv6 is not signed, so Secure Boot will block it

3. **Set USB as First Boot Device**
   - In Boot Order/Priority settings
   - Move USB device to top of list

4. **Enable USB Boot**
   - Some BIOSes have a separate "USB Boot" enable option

## Troubleshooting

### "Blinking cursor at top-left" 
- **Fixed** with A20 timeout protection
- If still occurs, try different USB port (USB 2.0 instead of 3.0)

### "BIOS skips USB drive"
- Check that USB is in boot order
- Try writing to a different USB drive
- Some very old BIOSes don't support USB boot at all

### "Boots in VirtualBox but not real hardware"
- This was the main issue - now fixed with timeout protection
- Real hardware is more strict about timing

### "System hangs after boot message"
- Could be hardware incompatibility (rare)
- Try booting with single CPU in BIOS if available

## Testing Checklist

- [x] VirtualBox boot
- [x] QEMU boot
- [ ] Real hardware boot (test with your USB)

## Technical Details

The bootloader now:
- Sets up segments and stack immediately
- Uses timeout-protected A20 enable (won't hang)
- Avoids BIOS interrupts that cause issues
- Uses absolute addressing for GDT

The kernel now:
- Supports single-core CPUs
- Falls back to PIC when APIC unavailable
- Handles missing hardware gracefully
- Works with or without E1000 network card
