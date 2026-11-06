
#ifndef PINECONE_ATA_H
#define PINECONE_ATA_H

#include <stdint.h>

#define ATA_SECTOR_SIZE 512
#define ATA_MAX_DEVICES 4

#define ATA_DEVICE_ATA   0
#define ATA_DEVICE_ATAPI 1

typedef struct
{
    uint8_t present;
    uint8_t channel;
    uint8_t drive;
    uint8_t type;

    uint16_t signature;
    uint16_t capabilities;

    uint32_t command_sets;
    uint32_t size;

    char model[41];

} ata_device_t;

/*
 * Initialize and detect ATA devices.
 */
void ata_initialize(void);

/*
 * Return the number of detected devices.
 */
uint8_t ata_get_device_count(void);

/*
 * Retrieve information about a detected device.
 */
const ata_device_t* ata_get_device(uint8_t index);

/*
 * Read one 512-byte sector.
 *
 * Returns:
 *  0 = success
 * -1 = failure
 */
int ata_read_sector(
    uint8_t device_index,
    uint32_t lba,
    uint16_t* buffer
);

#endif
