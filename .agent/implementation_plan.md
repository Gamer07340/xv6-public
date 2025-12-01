# Implementation Plan: Login System with SHA256 Password Hashing

## Objective
Implement automatic login on boot with user/password authentication and SHA256 password hashing.

## Tasks

### 1. Create SHA256 Implementation
- [ ] Create `sha256.h` header file with SHA256 function declarations
- [ ] Create `sha256.c` implementation file with SHA256 algorithm
- [ ] Add SHA256 to kernel build (for passwd management)
- [ ] Add SHA256 to user programs build (for login/su)

### 2. Create /root Directory
- [ ] Modify `mkfs.c` to create `/root` directory
- [ ] Set proper ownership (uid=0, gid=0) for root directory

### 3. Update Password Storage Format
- [ ] Modify `passwd.c` to store SHA256 hashes instead of plaintext
- [ ] Update password file format: `username:sha256_hash:uid:gid:home:shell`
- [ ] Create helper functions to hash and verify passwords

### 4. Modify init.c for Login on Boot
- [ ] Change `init.c` to exec `login` instead of `sh` directly
- [ ] Ensure login prompts for username/password before starting shell

### 5. Update login.c
- [ ] Modify to use SHA256 password verification
- [ ] Properly authenticate users before starting their shell

### 6. Update su.c
- [ ] Modify to use SHA256 password verification
- [ ] Authenticate before switching users

### 7. Update passwd_cmd.c
- [ ] Modify to hash passwords with SHA256 before storing
- [ ] Update password change functionality

### 8. Update useradd.c
- [ ] Modify to hash passwords with SHA256 when creating users
- [ ] Set default hashed password for new users

## Files to Modify
- `mkfs.c` - Add /root directory
- `sha256.h` (new) - SHA256 declarations
- `sha256.c` (new) - SHA256 implementation
- `passwd.c` - Password management with SHA256
- `init.c` - Launch login instead of sh
- `login.c` - Use SHA256 verification
- `su.c` - Use SHA256 verification
- `passwd_cmd.c` - Hash passwords with SHA256
- `useradd.c` - Hash passwords with SHA256
- `Makefile` - Add SHA256 to build

## Implementation Order
1. Create SHA256 implementation
2. Create /root directory in mkfs.c
3. Update passwd.c with SHA256 support
4. Update login.c for SHA256 authentication
5. Update su.c for SHA256 authentication
6. Update passwd_cmd.c for SHA256 hashing
7. Update useradd.c for SHA256 hashing
8. Modify init.c to use login
9. Update default root password in mkfs.c with SHA256 hash
