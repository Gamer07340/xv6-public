#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>

// xv6 types
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

// xv6 fs.h constants and structs (renamed to avoid conflicts)
#define XV6_ROOTINO 1
#define XV6_BSIZE 512
#define XV6_NDIRECT 11
#define XV6_NINDIRECT (XV6_BSIZE / sizeof(uint))
#define XV6_MAXFILE (XV6_NDIRECT + XV6_NINDIRECT + XV6_NINDIRECT * XV6_NINDIRECT)
#define XV6_IPB (XV6_BSIZE / sizeof(struct xv6_dinode))
#define XV6_DIRSIZ 14

struct xv6_superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

struct xv6_dinode {
  short type;           // File type
  short major;          // Major device number (T_DEV only)
  short minor;          // Minor device number (T_DEV only)
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  uint addrs[XV6_NDIRECT+2];   // Data block addresses
};

struct xv6_dirent {
  ushort inum;
  char name[XV6_DIRSIZ];
};

// xv6 file types
#define XV6_T_DIR  1
#define XV6_T_FILE 2
#define XV6_T_DEV  3

// Globals
void *disk_image = NULL;
size_t disk_size = 0;
struct xv6_superblock *sb = NULL;

#define BPB (XV6_BSIZE*8)
#define min(a, b) ((a) < (b) ? (a) : (b))

// Helper to get block pointer
void *get_block(uint bno) {
    if (bno >= sb->size) return NULL;
    return (char*)disk_image + (bno * XV6_BSIZE);
}

// Helper to zero a block
void xv6_bzero(uint bno) {
    void *blk = get_block(bno);
    if (blk) memset(blk, 0, XV6_BSIZE);
}

// Allocate a block
uint balloc() {
    int b, bi, m;
    uchar *bp; // bitmap block

    for(b = 0; b < sb->size; b += BPB){
        bp = (uchar*)get_block(sb->bmapstart + b/BPB);
        for(bi = 0; bi < BPB && b + bi < sb->size; bi++){
            m = 1 << (bi % 8);
            if((bp[bi/8] & m) == 0){  // Is block free?
                bp[bi/8] |= m;  // Mark block in use.
                xv6_bzero(b + bi);
                return b + bi;
            }
        }
    }
    return 0;
}

// Free a block
void bfree(uint b) {
    uchar *bp = (uchar*)get_block(sb->bmapstart + b/BPB);
    int bi = b % BPB;
    int m = 1 << (bi % 8);
    if((bp[bi/8] & m) == 0)
        return; // Already free
    bp[bi/8] &= ~m;
}

// Helper to get inode
struct xv6_dinode *get_inode(uint inum) {
    if (inum < 1 || inum >= sb->ninodes) return NULL;
    uint block_group = (inum / XV6_IPB);
    uint block_offset = (inum % XV6_IPB);
    uint bno = sb->inodestart + block_group;
    
    struct xv6_dinode *dip_block = (struct xv6_dinode *)get_block(bno);
    if (!dip_block) return NULL;
    return &dip_block[block_offset];
}

// Helper to get inum from inode pointer
uint get_inum(struct xv6_dinode *ip) {
    void *start = get_block(sb->inodestart);
    if ((void*)ip < start) return 0;
    ptrdiff_t diff = (char*)ip - (char*)start;
    return diff / sizeof(struct xv6_dinode);
}

// Truncate inode (discard contents)
void itrunc(struct xv6_dinode *ip) {
    int i, j;
    uint *a;
    void *bp;

    for(i = 0; i < XV6_NDIRECT; i++){
        if(ip->addrs[i]){
            bfree(ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if(ip->addrs[XV6_NDIRECT]){
        bp = get_block(ip->addrs[XV6_NDIRECT]);
        if (bp) {
            a = (uint*)bp;
            for(j = 0; j < XV6_NINDIRECT; j++){
                if(a[j])
                    bfree(a[j]);
            }
        }
        bfree(ip->addrs[XV6_NDIRECT]);
        ip->addrs[XV6_NDIRECT] = 0;
    }

    if(ip->addrs[XV6_NDIRECT+1]){
        bp = get_block(ip->addrs[XV6_NDIRECT+1]);
        if (bp) {
            a = (uint*)bp;
            for(j = 0; j < XV6_NINDIRECT; j++){
                if(a[j]){
                    void *bp2 = get_block(a[j]);
                    if (bp2) {
                        uint *a2 = (uint*)bp2;
                        for(int k = 0; k < XV6_NINDIRECT; k++){
                            if(a2[k])
                                bfree(a2[k]);
                        }
                    }
                    bfree(a[j]);
                }
            }
        }
        bfree(ip->addrs[XV6_NDIRECT+1]);
        ip->addrs[XV6_NDIRECT+1] = 0;
    }

    ip->size = 0;
}

// Allocate inode
uint ialloc(short type) {
    int inum;
    struct xv6_dinode *dip;

    for(inum = 1; inum < sb->ninodes; inum++){
        dip = get_inode(inum);
        if(dip->type == 0){  // a free inode
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            return inum;
        }
    }
    return 0;
}

// Helper to map logical block number to physical block number
uint bmap(struct xv6_dinode *ip, uint bn, int alloc) {
    uint addr, *a;
    
    if(bn < XV6_NDIRECT){
        if((addr = ip->addrs[bn]) == 0) {
            if (!alloc) return 0;
            ip->addrs[bn] = addr = balloc();
        }
        return addr;
    }
    bn -= XV6_NDIRECT;

    if(bn < XV6_NINDIRECT){
        if((addr = ip->addrs[XV6_NDIRECT]) == 0) {
            if (!alloc) return 0;
            ip->addrs[XV6_NDIRECT] = addr = balloc();
        }
        a = (uint*)get_block(addr);
        if (!a) return 0;
        if((addr = a[bn]) == 0){
            if (!alloc) return 0;
            a[bn] = addr = balloc();
        }
        return addr;
    }
    bn -= XV6_NINDIRECT;

    if(bn < XV6_NINDIRECT * XV6_NINDIRECT){
        if((addr = ip->addrs[XV6_NDIRECT+1]) == 0) {
            if (!alloc) return 0;
            ip->addrs[XV6_NDIRECT+1] = addr = balloc();
        }
        a = (uint*)get_block(addr);
        if (!a) return 0;
        addr = a[bn / XV6_NINDIRECT];
        if (addr == 0) {
            if (!alloc) return 0;
            a[bn / XV6_NINDIRECT] = addr = balloc();
            addr = a[bn / XV6_NINDIRECT];
        }
        a = (uint*)get_block(addr);
        if (!a) return 0;
        if((addr = a[bn % XV6_NINDIRECT]) == 0){
            if (!alloc) return 0;
            a[bn % XV6_NINDIRECT] = addr = balloc();
        }
        return addr;
    }

    return 0;
}

// Helper to resolve path to inode
struct xv6_dinode *namei(const char *path) {
    struct xv6_dinode *ip = get_inode(XV6_ROOTINO);
    if (strcmp(path, "/") == 0) return ip;

    char *path_copy = strdup(path);
    char *token = strtok(path_copy, "/");
    
    while (token != NULL) {
        if (ip->type != XV6_T_DIR) {
            free(path_copy);
            return NULL;
        }

        int found = 0;
        for (uint off = 0; off < ip->size; off += sizeof(struct xv6_dirent)) {
            struct xv6_dirent de;
            uint bno = bmap(ip, off / XV6_BSIZE, 0);
            if (bno == 0) continue;
            void *blk = get_block(bno);
            memcpy(&de, (char*)blk + (off % XV6_BSIZE), sizeof(de));
            
            if (de.inum == 0) continue;
            if (strncmp(de.name, token, XV6_DIRSIZ) == 0) {
                ip = get_inode(de.inum);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            free(path_copy);
            return NULL;
        }
        
        token = strtok(NULL, "/");
    }
    free(path_copy);
    return ip;
}

// Helper to resolve parent directory and name
struct xv6_dinode *nameiparent(const char *path, char *name) {
    struct xv6_dinode *ip = get_inode(XV6_ROOTINO);
    
    char *path_copy = strdup(path);
    // Handle root
    if (strcmp(path, "/") == 0) {
        free(path_copy);
        return NULL;
    }

    // Split path
    char *last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        strncpy(name, last_slash + 1, XV6_DIRSIZ);
        if (last_slash == path_copy) {
           // Parent is root
           ip = get_inode(XV6_ROOTINO);
        } else {
           ip = namei(path_copy);
        }
    } else {
        // Should not happen for absolute paths passed by FUSE
        strncpy(name, path_copy, XV6_DIRSIZ);
    }
    free(path_copy);
    return ip;
}

// Add entry to directory
int dirlink(struct xv6_dinode *dp, char *name, uint inum) {
    int off;
    struct xv6_dirent de;
    
    // Look for empty dirent
    for(off = 0; off < dp->size; off += sizeof(de)){
        uint bno = bmap(dp, off / XV6_BSIZE, 0);
        if (bno) {
            void *blk = get_block(bno);
            memcpy(&de, (char*)blk + (off % XV6_BSIZE), sizeof(de));
            if(de.inum == 0)
                break;
        }
    }

    memset(&de, 0, sizeof(de));
    strncpy(de.name, name, XV6_DIRSIZ);
    de.inum = inum;
    
    uint bno = bmap(dp, off / XV6_BSIZE, 1);
    if (bno == 0) return -1;
    
    void *blk = get_block(bno);
    memcpy((char*)blk + (off % XV6_BSIZE), &de, sizeof(de));
    
    if(off >= dp->size)
        dp->size = off + sizeof(de);
        
    return 0;
}

// FUSE Operations

static int xv6_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    
    struct xv6_dinode *ip = namei(path);
    if (ip == NULL) return -ENOENT;

    if (ip->type == XV6_T_DIR) {
        stbuf->st_mode = S_IFDIR | 0755;
    } else if (ip->type == XV6_T_FILE) {
        stbuf->st_mode = S_IFREG | 0644;
    } else {
        stbuf->st_mode = S_IFREG | 0644; 
    }
    stbuf->st_nlink = ip->nlink;
    stbuf->st_size = ip->size;

    return 0;
}

static int xv6_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi,
                       enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    struct xv6_dinode *ip = namei(path);
    if (ip == NULL) return -ENOENT;

    if (ip->type != XV6_T_DIR) return -ENOTDIR;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    for (uint off = 0; off < ip->size; off += sizeof(struct xv6_dirent)) {
        struct xv6_dirent de;
        uint bno = bmap(ip, off / XV6_BSIZE, 0);
        if (bno == 0) continue;
        void *blk = get_block(bno);
        memcpy(&de, (char*)blk + (off % XV6_BSIZE), sizeof(de));
        
        if (de.inum == 0) continue;
        
        // Ensure null termination for name
        char name[XV6_DIRSIZ + 1];
        strncpy(name, de.name, XV6_DIRSIZ);
        name[XV6_DIRSIZ] = '\0';
        
        filler(buf, name, NULL, 0, 0);
    }

    return 0;
}

static int xv6_open(const char *path, struct fuse_file_info *fi) {
    // We can just check existence here, or do nothing as getattr checks it.
    // But we should check flags. Read-only for now?
    // if ((fi->flags & 3) != O_RDONLY) return -EACCES;
    return 0;
}

static int xv6_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;
    
    struct xv6_dinode *ip = namei(path);
    if (ip == NULL) return -ENOENT;

    if (offset >= ip->size) return 0;
    if (offset + size > ip->size) size = ip->size - offset;

    size_t bytes_read = 0;
    while (bytes_read < size) {
        uint bno = bmap(ip, (offset + bytes_read) / XV6_BSIZE, 0);
        if (bno == 0) break; // Should not happen if size is correct
        
        void *blk = get_block(bno);
        size_t block_off = (offset + bytes_read) % XV6_BSIZE;
        size_t to_copy = XV6_BSIZE - block_off;
        if (to_copy > size - bytes_read) to_copy = size - bytes_read;
        
        memcpy(buf + bytes_read, (char*)blk + block_off, to_copy);
        bytes_read += to_copy;
    }

    return bytes_read;
}

static int xv6_write(const char *path, const char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi) {
    (void) fi;
    
    struct xv6_dinode *ip = namei(path);
    if (ip == NULL) return -ENOENT;

    // XV6 max file size check
    if (offset + size > XV6_MAXFILE * XV6_BSIZE)
        return -EFBIG;

    size_t bytes_written = 0;
    while (bytes_written < size) {
        uint bno = bmap(ip, (offset + bytes_written) / XV6_BSIZE, 1);
        if (bno == 0) return -ENOSPC;
        
        void *blk = get_block(bno);
        size_t block_off = (offset + bytes_written) % XV6_BSIZE;
        size_t to_copy = XV6_BSIZE - block_off;
        if (to_copy > size - bytes_written) to_copy = size - bytes_written;
        
        memcpy((char*)blk + block_off, buf + bytes_written, to_copy);
        bytes_written += to_copy;
    }

    if (offset + bytes_written > ip->size)
        ip->size = offset + bytes_written;

    return bytes_written;
}

static int xv6_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;
    (void) fi;
    
    char name[XV6_DIRSIZ];
    struct xv6_dinode *dp = nameiparent(path, name);
    if (dp == NULL) return -ENOENT;

    // Check if exists
    // (omitted)

    uint inum = ialloc(XV6_T_FILE);
    if (inum == 0) return -ENOSPC;
    
    struct xv6_dinode *ip = get_inode(inum);
    ip->nlink = 1;
    ip->major = 0;
    ip->minor = 0;
    ip->size = 0;
    
    if (dirlink(dp, name, inum) < 0) {
        // Free inode?
        ip->type = 0;
        return -ENOSPC;
    }
    
    return 0;
}

static int xv6_mkdir(const char *path, mode_t mode) {
    (void) mode;
    char name[XV6_DIRSIZ];
    struct xv6_dinode *dp = nameiparent(path, name);
    if (dp == NULL) return -ENOENT;

    uint inum = ialloc(XV6_T_DIR);
    if (inum == 0) return -ENOSPC;
    
    struct xv6_dinode *ip = get_inode(inum);
    ip->nlink = 1;
    ip->size = 0;
    
    // . and ..
    if (dirlink(ip, ".", inum) < 0 || dirlink(ip, "..", get_inum(dp)) < 0) {
         ip->type = 0; // Failed, free inode (leaks blocks if allocated)
         return -ENOSPC;
    }
    
    if (dirlink(dp, name, inum) < 0) {
        ip->type = 0;
        return -ENOSPC;
    }
    
    return 0;
}

static int xv6_unlink(const char *path) {
    char name[XV6_DIRSIZ];
    struct xv6_dinode *dp = nameiparent(path, name);
    if (dp == NULL) return -ENOENT;

    // Find entry in dp
    for (uint off = 0; off < dp->size; off += sizeof(struct xv6_dirent)) {
        struct xv6_dirent de;
        uint bno = bmap(dp, off / XV6_BSIZE, 0);
        if (bno == 0) continue;
        void *blk = get_block(bno);
        memcpy(&de, (char*)blk + (off % XV6_BSIZE), sizeof(de));
        
        if (de.inum == 0) continue;
        if (strncmp(de.name, name, XV6_DIRSIZ) == 0) {
            // Found it.
            struct xv6_dinode *ip = get_inode(de.inum);
            
            // Clear dirent
            memset((char*)blk + (off % XV6_BSIZE), 0, sizeof(de));
            
            // Decrement link count
            if (ip->nlink > 0) ip->nlink--;
            if (ip->nlink == 0) {
                itrunc(ip);
                ip->type = 0; // Mark free
            }
            return 0;
        }
    }
    return -ENOENT;
}

static int xv6_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
    (void) fi;
    struct xv6_dinode *ip = namei(path);
    if (ip == NULL) return -ENOENT;
    
    if (size < ip->size) {
        // Shrink - simplified, only supports full truncate to 0 for now or just size update
        // If size is 0, we can itrunc
        if (size == 0) {
            itrunc(ip);
        } else {
            // TODO: partial truncate
            ip->size = size;
        }
    } else if (size > ip->size) {
        // Expand (holes)
        ip->size = size;
    }
    return 0;
}

static struct fuse_operations xv6_oper = {
    .getattr    = xv6_getattr,
    .readdir    = xv6_readdir,
    .open       = xv6_open,
    .read       = xv6_read,
    .write      = xv6_write,
    .create     = xv6_create,
    .mkdir      = xv6_mkdir,
    .unlink     = xv6_unlink,
    .truncate   = xv6_truncate,
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <fs.img> <mountpoint>\n", argv[0]);
        return 1;
    }

    // Map the fs image
    int fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        return 1;
    }
    disk_size = st.st_size;

    disk_image = mmap(NULL, disk_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_image == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    
    sb = (struct xv6_superblock *)((char*)disk_image + XV6_BSIZE); // Superblock is at block 1

    printf("Superblock: size=%d nblocks=%d ninodes=%d\n", sb->size, sb->nblocks, sb->ninodes);

    // Adjust args for fuse_main
    // fuse_main expects argv[0] to be program name, and then options/mountpoint
    // We consumed argv[1] as image.
    // So we need to shift args.
    
    char **fuse_argv = malloc(sizeof(char*) * argc);
    fuse_argv[0] = argv[0];
    for(int i=2; i<argc; i++) {
        fuse_argv[i-1] = argv[i];
    }
    int fuse_argc = argc - 1;

    return fuse_main(fuse_argc, fuse_argv, &xv6_oper, NULL);
}
