/**
 * @file    gc9a01_config.h
 * @brief   GC9A01 RAM/吞吐参数（可在编译前 #define 覆盖）。
 */

#ifndef GC9A01_CONFIG_H
#define GC9A01_CONFIG_H

#ifndef GC9A01_TX_CHUNK
#define GC9A01_TX_CHUNK 512U
#endif

#ifndef GC9A01_LINEBUF_MAX
#define GC9A01_LINEBUF_MAX 240U
#endif

#endif /* GC9A01_CONFIG_H */
