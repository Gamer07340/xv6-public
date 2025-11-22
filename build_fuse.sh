#!/bin/bash
if ! command -v pkg-config &> /dev/null; then
    echo "pkg-config could not be found. Please install it."
    exit 1
fi

if ! pkg-config --exists fuse; then
    if ! pkg-config --exists fuse3; then
        echo "FUSE development libraries not found. Please install libfuse-dev or libfuse3-dev."
        exit 1
    else
        FUSE_LIB="fuse3"
    fi
else
    FUSE_LIB="fuse"
fi

echo "Compiling xv6fs_fuse using $FUSE_LIB..."
gcc -Wall xv6fs_fuse.c `pkg-config $FUSE_LIB --cflags --libs` -o xv6fs_fuse
if [ $? -eq 0 ]; then
    echo "Compilation successful. Run with: ./xv6fs_fuse fs.img <mountpoint>"
else
    echo "Compilation failed."
fi
