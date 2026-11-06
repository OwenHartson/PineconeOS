
#include <stdint.h>

#include "../include/fat32.h"
#include "../include/ata.h"
#include "../include/terminal.h"

#define FAT32_SECTOR_SIZE       512
#define FAT32_PARTITION_START   446

#define FAT32_ATTR_DIRECTORY    0x10
#define FAT32_ATTR_VOLUME_ID    0x08
#define FAT32_ATTR_LONG_NAME    0x0F

#define FAT32_EOC               0x0FFFFFF8
#define FAT32_BAD_CLUSTER       0x0FFFFFF7
#define FAT32_CLUSTER_MASK      0x0FFFFFFF

#define FAT32_MAX_CLUSTERS      0x0FFFFFF5

typedef struct
{
    uint32_t volume_start;
    uint32_t total_sectors;

    uint32_t fat_start;
    uint32_t data_start;

    uint32_t sectors_per_fat;
    uint32_t cluster_count;

    uint32_t root_cluster;

    uint8_t sectors_per_cluster;
    uint8_t mounted;
    uint8_t device_index;

} fat32_volume_t;

static fat32_volume_t volume;

static uint16_t sector_buffer[256];


/*
 * Read little-endian 16-bit value.
 */
static uint16_t read_u16(const uint8_t* data)
{
    return (uint16_t)data[0] |
           ((uint16_t)data[1] << 8);
}


/*
 * Read little-endian 32-bit value.
 */
static uint32_t read_u32(const uint8_t* data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}


/*
 * Read a sector from the mounted ATA device.
 */
static int fat32_read_sector(uint32_t lba)
{
    if (volume.mounted == 0)
        return -1;

    if (lba < volume.volume_start ||
        lba >= volume.volume_start + volume.total_sectors)
    {
        return -1;
    }

    return ata_read_sector(
        volume.device_index,
        lba,
        sector_buffer
    );
}


/*
 * Get the absolute sector of a cluster.
 */
static uint32_t fat32_cluster_sector(uint32_t cluster)
{
    return volume.data_start +
           ((cluster - 2) * volume.sectors_per_cluster);
}


/*
 * Validate a cluster number.
 */
static int fat32_valid_cluster(uint32_t cluster)
{
    return cluster >= 2 &&
           cluster < volume.cluster_count + 2;
}


/*
 * Read the next cluster from the FAT.
 */
static int fat32_next_cluster(
    uint32_t cluster,
    uint32_t* next_cluster
)
{
    if (!fat32_valid_cluster(cluster) || next_cluster == 0)
        return -1;

    uint32_t fat_offset = cluster * 4;

    uint32_t fat_sector =
        volume.fat_start +
        (fat_offset / FAT32_SECTOR_SIZE);

    uint32_t entry_offset =
        fat_offset % FAT32_SECTOR_SIZE;

    if (fat_sector >=
        volume.fat_start + volume.sectors_per_fat)
    {
        return -1;
    }

    if (fat32_read_sector(fat_sector) != 0)
        return -1;

    uint8_t* data = (uint8_t*)sector_buffer;

    uint32_t value = read_u32(&data[entry_offset]);

    value &= FAT32_CLUSTER_MASK;

    if (value >= FAT32_EOC)
        return 1;

    if (value == FAT32_BAD_CLUSTER ||
        !fat32_valid_cluster(value))
    {
        return -1;
    }

    *next_cluster = value;

    return 0;
}


/*
 * Check the FAT32 boot sector.
 */
static int fat32_parse_boot_sector(uint32_t start_sector)
{
    /*
     * Read the candidate boot sector.
     */
    volume.volume_start = start_sector;
    volume.total_sectors = 0;
    volume.mounted = 0;

    /*
     * Temporarily allow the first boot-sector read.
     */
    if (ata_read_sector(
        volume.device_index,
        start_sector,
        sector_buffer
    ) != 0)
    {
        return -1;
    }

    uint8_t* bpb = (uint8_t*)sector_buffer;

    /*
     * Check boot sector signature.
     */
    if (bpb[510] != 0x55 || bpb[511] != 0xAA)
        return -1;

    uint16_t bytes_per_sector = read_u16(&bpb[11]);

    uint8_t sectors_per_cluster = bpb[13];

    uint16_t reserved_sectors = read_u16(&bpb[14]);

    uint8_t number_of_fats = bpb[16];

    uint16_t root_entry_count = read_u16(&bpb[17]);

    uint16_t total_sectors_16 = read_u16(&bpb[19]);

    uint16_t fat_size_16 = read_u16(&bpb[22]);

    uint32_t total_sectors_32 = read_u32(&bpb[32]);

    uint32_t fat_size_32 = read_u32(&bpb[36]);

    uint32_t root_cluster = read_u32(&bpb[44]);

    /*
     * FAT32 requires 512-byte sectors for this driver.
     */
    if (bytes_per_sector != FAT32_SECTOR_SIZE)
        return -1;

    /*
     * Validate cluster size.
     */
    if (sectors_per_cluster == 0 ||
        (sectors_per_cluster &
        (sectors_per_cluster - 1)) != 0)
    {
        return -1;
    }

    if (reserved_sectors == 0 ||
        number_of_fats == 0 ||
        number_of_fats > 2 ||
        root_entry_count != 0 ||
        fat_size_16 != 0 ||
        fat_size_32 == 0 ||
        total_sectors_16 != 0 ||
        total_sectors_32 == 0)
    {
        return -1;
    }

    /*
     * Calculate filesystem layout.
     */
    uint32_t total_sectors = total_sectors_32;

    uint32_t first_data_sector =
        reserved_sectors +
        ((uint32_t)number_of_fats * fat_size_32);

    if (first_data_sector >= total_sectors)
        return -1;

    uint32_t data_sectors =
        total_sectors - first_data_sector;

    uint32_t cluster_count =
        data_sectors / sectors_per_cluster;

    if (cluster_count == 0 ||
        cluster_count > FAT32_MAX_CLUSTERS)
    {
        return -1;
    }

    if (root_cluster < 2 ||
        root_cluster >= cluster_count + 2)
    {
        return -1;
    }

    /*
     * Validate the partition against the ATA device.
     */
    const ata_device_t* device =
        ata_get_device(volume.device_index);

    if (device == 0)
        return -1;

    if (start_sector >= device->size ||
        total_sectors > device->size - start_sector)
    {
        return -1;
    }

    /*
     * Save filesystem information.
     */
    volume.volume_start = start_sector;
    volume.total_sectors = total_sectors;

    volume.fat_start =
        start_sector + reserved_sectors;

    volume.data_start =
        start_sector + first_data_sector;

    volume.sectors_per_fat = fat_size_32;
    volume.cluster_count = cluster_count;

    volume.root_cluster = root_cluster;

    volume.sectors_per_cluster = sectors_per_cluster;

    volume.mounted = 1;

    return 0;
}


/*
 * Mount a FAT32 filesystem.
 */
int fat32_mount(uint8_t device_index)
{
    volume.mounted = 0;

    const ata_device_t* device =
        ata_get_device(device_index);

    if (device == 0)
    {
        terminal_write("[FAT32] Invalid ATA device.\n");
        return -1;
    }

    volume.device_index = device_index;

    terminal_write("[FAT32] Searching for FAT32 filesystem...\n");

    /*
     * First inspect the MBR.
     */
    if (ata_read_sector(
        device_index,
        0,
        sector_buffer
    ) != 0)
    {
        terminal_write("[FAT32] Cannot read disk sector 0.\n");
        return -1;
    }

    uint8_t* mbr = (uint8_t*)sector_buffer;

    /*
     * Check MBR signature.
     */
    if (mbr[510] == 0x55 && mbr[511] == 0xAA)
    {
        /*
         * Search the four primary partition entries.
         */
        for (uint32_t i = 0; i < 4; i++)
        {
            uint32_t offset =
                FAT32_PARTITION_START + (i * 16);

            uint8_t partition_type = mbr[offset + 4];

            uint32_t partition_start =
                read_u32(&mbr[offset + 8]);

            /*
             * FAT32 partition types:
             * 0x0B = FAT32
             * 0x0C = FAT32 LBA
             */
            if ((partition_type == 0x0B ||
                 partition_type == 0x0C) &&
                partition_start != 0)
            {
                if (fat32_parse_boot_sector(partition_start) == 0)
                {
                    terminal_write("[FAT32] MBR partition mounted.\n");
                    return 0;
                }
            }
        }
    }

    /*
     * Try a filesystem that starts directly at sector 0.
     */
    if (fat32_parse_boot_sector(0) == 0)
    {
        terminal_write("[FAT32] Whole-disk filesystem mounted.\n");
        return 0;
    }

    volume.mounted = 0;

    terminal_write("[FAT32] No supported FAT32 filesystem found.\n");

    return -1;
}


/*
 * Return mount state.
 */
uint8_t fat32_is_mounted(void)
{
    return volume.mounted;
}


/*
 * Convert an 8.3 directory name to a normal string.
 */
static void fat32_format_filename(
    const uint8_t* entry,
    char* output
)
{
    uint32_t position = 0;

    /*
     * Filename.
     */
    for (uint32_t i = 0; i < 8 && entry[i] != ' '; i++)
    {
        output[position++] = (char)entry[i];
    }

    /*
     * Extension.
     */
    uint32_t extension_length = 0;

    for (uint32_t i = 8; i < 11; i++)
    {
        if (entry[i] != ' ')
            extension_length++;
    }

    if (extension_length > 0)
    {
        output[position++] = '.';

        for (uint32_t i = 8; i < 11; i++)
        {
            if (entry[i] != ' ')
                output[position++] = (char)entry[i];
        }
    }

    output[position] = '\0';
}


/*
 * Compare two filenames without case sensitivity.
 */
static int fat32_compare_filename(
    const char* a,
    const char* b
)
{
    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;

        if (ca >= 'a' && ca <= 'z')
            ca -= 32;

        if (cb >= 'a' && cb <= 'z')
            cb -= 32;

        if (ca != cb)
            return 0;

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}


/*
 * Decode a directory entry.
 */
static void fat32_decode_entry(
    const uint8_t* entry,
    fat32_file_t* file
)
{
    fat32_format_filename(entry, file->name);

    file->attributes = entry[11];

    file->first_cluster =
        ((uint32_t)read_u16(&entry[20]) << 16) |
        read_u16(&entry[26]);

    file->size = read_u32(&entry[28]);
}


/*
 * Find a file in the root directory.
 */
int fat32_find_file(
    const char* filename,
    fat32_file_t* result
)
{
    if (!volume.mounted || !filename || !result)
        return -1;

    uint32_t cluster = volume.root_cluster;

    /*
     * Limit traversal to prevent infinite loops
     * caused by a corrupted FAT.
     */
    for (uint32_t count = 0;
         count < volume.cluster_count;
         count++)
    {
        uint32_t first_sector =
            fat32_cluster_sector(cluster);

        for (uint32_t s = 0;
             s < volume.sectors_per_cluster;
             s++)
        {
            if (fat32_read_sector(first_sector + s) != 0)
                return -1;

            uint8_t* data = (uint8_t*)sector_buffer;

            for (uint32_t offset = 0;
                 offset < FAT32_SECTOR_SIZE;
                 offset += 32)
            {
                uint8_t* entry = &data[offset];

                if (entry[0] == 0x00)
                    return -1;

                if (entry[0] == 0xE5)
                    continue;

                if (entry[11] == FAT32_ATTR_LONG_NAME)
                    continue;

                if (entry[11] & FAT32_ATTR_VOLUME_ID)
                    continue;

                fat32_file_t candidate;

                fat32_decode_entry(entry, &candidate);

                if (fat32_compare_filename(
                    filename,
                    candidate.name
                ))
                {
                    *result = candidate;
                    return 0;
                }
            }
        }

        uint32_t next;

        int status = fat32_next_cluster(cluster, &next);

        if (status != 0)
            return -1;

        cluster = next;
    }

    return -1;
}


/*
 * List the root directory.
 */
int fat32_list_root(void)
{
    if (!volume.mounted)
        return -1;

    terminal_write("\nPINECONEOS ROOT DIRECTORY\n");
    terminal_write("=========================\n");

    uint32_t cluster = volume.root_cluster;

    for (uint32_t count = 0;
         count < volume.cluster_count;
         count++)
    {
        uint32_t first_sector =
            fat32_cluster_sector(cluster);

        for (uint32_t s = 0;
             s < volume.sectors_per_cluster;
             s++)
        {
            if (fat32_read_sector(first_sector + s) != 0)
                return -1;

            uint8_t* data = (uint8_t*)sector_buffer;

            for (uint32_t offset = 0;
                 offset < FAT32_SECTOR_SIZE;
                 offset += 32)
            {
                uint8_t* entry = &data[offset];

                if (entry[0] == 0x00)
                    return 0;

                if (entry[0] == 0xE5)
                    continue;

                if (entry[11] == FAT32_ATTR_LONG_NAME)
                    continue;

                if (entry[11] & FAT32_ATTR_VOLUME_ID)
                    continue;

                fat32_file_t file;

                fat32_decode_entry(entry, &file);

                terminal_write(file.name);

                if (file.attributes & FAT32_ATTR_DIRECTORY)
                    terminal_write("/");

                terminal_write("\n");
            }
        }

        uint32_t next;

        int status = fat32_next_cluster(cluster, &next);

        if (status != 0)
            return status == 1 ? 0 : -1;

        cluster = next;
    }

    return -1;
}


/*
 * Read a file from the root directory.
 *
 * Files larger than the caller's buffer are rejected.
 */
int fat32_read_file(
    const char* filename,
    void* buffer,
    uint32_t capacity,
    uint32_t* bytes_read
)
{
    if (bytes_read)
        *bytes_read = 0;

    if (!volume.mounted ||
        !filename ||
        !buffer ||
        !bytes_read)
    {
        return -1;
    }

    fat32_file_t file;

    if (fat32_find_file(filename, &file) != 0)
        return -1;

    /*
     * Directories cannot be read as files.
     */
    if (file.attributes & FAT32_ATTR_DIRECTORY)
        return -1;

    if (file.size > capacity)
        return -1;

    if (file.size == 0)
        return 0;

    if (!fat32_valid_cluster(file.first_cluster))
        return -1;

    uint8_t* destination = (uint8_t*)buffer;

    uint32_t remaining = file.size;
    uint32_t cluster = file.first_cluster;

    for (uint32_t count = 0;
         count < volume.cluster_count && remaining > 0;
         count++)
    {
        uint32_t first_sector =
            fat32_cluster_sector(cluster);

        for (uint32_t s = 0;
             s < volume.sectors_per_cluster && remaining > 0;
             s++)
        {
            if (fat32_read_sector(first_sector + s) != 0)
                return -1;

            uint32_t amount = remaining;

            if (amount > FAT32_SECTOR_SIZE)
                amount = FAT32_SECTOR_SIZE;

            uint8_t* source = (uint8_t*)sector_buffer;

            for (uint32_t i = 0; i < amount; i++)
            {
                destination[i] = source[i];
            }

            destination += amount;
            remaining -= amount;
        }

        if (remaining == 0)
            break;

        uint32_t next;

        int status = fat32_next_cluster(cluster, &next);

        if (status != 0)
            return -1;

        cluster = next;
    }

    if (remaining != 0)
        return -1;

    *bytes_read = file.size;

    return 0;
}
