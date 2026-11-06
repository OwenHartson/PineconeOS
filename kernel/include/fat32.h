
#ifndef PINECONE_FAT32_H
#define PINECONE_FAT32_H

#include <stdint.h>

#define FAT32_FILENAME_LENGTH 13

typedef struct
{
    char name[FAT32_FILENAME_LENGTH];
    uint32_t size;
    uint8_t attributes;
    uint32_t first_cluster;

} fat32_file_t;

/*
 * Mount a FAT32 filesystem on an ATA device.
 *
 * Returns:
 *  0 = success
 * -1 = failure
 */
int fat32_mount(uint8_t device_index);

/*
 * Check whether a FAT32 filesystem is mounted.
 */
uint8_t fat32_is_mounted(void);

/*
 * Print the root directory contents.
 */
int fat32_list_root(void);

/*
 * Find a file in the root directory.
 */
int fat32_find_file(
    const char* filename,
    fat32_file_t* result
);

/*
 * Read a file into a caller-provided buffer.
 *
 * Returns:
 *  0 = success
 * -1 = failure
 *
 * bytes_read receives the actual file size.
 */
int fat32_read_file(
    const char* filename,
    void* buffer,
    uint32_t capacity,
    uint32_t* bytes_read
);

#endif
