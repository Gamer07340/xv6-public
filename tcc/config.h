#ifndef _CONFIG_H
#define _CONFIG_H

#define TCC_VERSION "0.9.27"
#ifdef TCC_TARGET_I386
#define CONFIG_TCCDIR "/lib/tcc"
#define CONFIG_TCC_SYSINCLUDEPATHS "/usr/include:/usr/local/include:{B}/include"
#endif

#define GCC_MAJOR 4
#define GCC_MINOR 2
#define CC_NAME "gcc"

#endif
