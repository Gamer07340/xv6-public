# DNS Fix for QEMU User-Mode Networking

## Problem
DNS queries to external servers (like 8.8.8.8) were failing with "recv failed or too short" because QEMU's user-mode networking blocks or doesn't properly forward external DNS queries.

## Solution
Updated `dns.c` to use QEMU's built-in DNS server at **10.0.2.3** instead of 8.8.8.8.

QEMU user-mode networking provides a built-in DNS server that:
- Forwards queries to the host's DNS resolver
- Works reliably within QEMU's network
- Is accessible at 10.0.2.3:53

## Changes Made
1. Try QEMU DNS server (10.0.2.3) first
2. Fall back to 8.8.8.8 if that fails
3. Added retry logic for DNS responses
4. Added debug output to track DNS queries

## Test
```bash
$ wget http://denaro.mine.bz/index.php
```

Should now successfully resolve the hostname and download the file.
