
#include <stdint.h>

#include "../include/ata.h"
#include "../include/io.h"
#include "../include/terminal.h"

#define ATA_PRIMARY_IO       0x1F0
#define ATA_PRIMARY_CTRL     0x3F6

#define ATA_SECONDARY_IO     0x170
#define ATA_SECONDARY_CTRL   0x376

#define ATA_REG_DATA         0
#define ATA_REG_ERROR        1
#define ATA_REG_SECCOUNT0    2
#define ATA_REG_LBA0         3
#define ATA_REG_LBA1         4
#define ATA_REG_LBA2         5
#define ATA_REG_HDDEVSEL     6
#define ATA_REG_COMMAND      7
#define ATA_REG_STATUS       7

#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_READ_PIO     0x20

#define ATA_STATUS_ERR       0x01
#define ATA_STATUS_DRQ       0x08
#define ATA_STATUS_DF        0x20
#define ATA_STATUS_BSY       0x80

#define ATA_TIMEOUT          1000000

typedef struct
{
    uint16_t base;
    uint16_t ctrl;

} ata_channel_t;

static ata_channel_t channels[2];

static ata_device_t devices[ATA_MAX_DEVICES];

static uint8_t device_count = 0;


/*
 * Read a 16-bit word from an I/O port.
 */
static uint16_t ata_inw(uint16_t port)
{
    uint16_t value;

    __asm__ volatile (
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}


/*
 * Wait approximately 400 nanoseconds.
 */
static void ata_delay(ata_channel_t* channel)
{
    inb(channel->ctrl);
    inb(channel->ctrl);
    inb(channel->ctrl);
    inb(channel->ctrl);
}


/*
 * Wait until the drive is no longer busy.
 */
static int ata_wait_not_busy(ata_channel_t* channel)
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = inb(channel->base + ATA_REG_STATUS);

        if (status == 0 || status == 0xFF)
            return -1;

        if (!(status & ATA_STATUS_BSY))
            return 0;
    }

    return -1;
}


/*
 * Wait until the drive is ready to transfer data.
 */
static int ata_wait_drq(ata_channel_t* channel)
{
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++)
    {
        uint8_t status = inb(channel->base + ATA_REG_STATUS);

        if (status == 0 || status == 0xFF)
            return -1;

        if (status & (ATA_STATUS_ERR | ATA_STATUS_DF))
            return -1;

        if (!(status & ATA_STATUS_BSY) &&
            (status & ATA_STATUS_DRQ))
        {
            return 0;
        }
    }

    return -1;
}


/*
 * Convert ATA's byte-swapped strings.
 */
static void ata_read_string(
    uint16_t* source,
    char* destination,
    uint32_t words
)
{
    for (uint32_t i = 0; i < words; i++)
    {
        destination[i * 2] =
            (char)(source[i] >> 8);

        destination[i * 2 + 1] =
            (char)(source[i] & 0xFF);
    }

    destination[words * 2] = '\0';

    /*
     * Remove trailing spaces.
     */
    for (int i = (int)(words * 2) - 1; i >= 0; i--)
    {
        if (destination[i] == ' ')
            destination[i] = '\0';
        else
            break;
    }
}


/*
 * Identify an ATA device.
 */
static int ata_identify(
    uint8_t channel_index,
    uint8_t drive,
    ata_device_t* device
)
{
    ata_channel_t* channel = &channels[channel_index];

    uint16_t identify_data[256];

    uint16_t base = channel->base;

    /*
     * Select master or slave.
     */
    outb(
        base + ATA_REG_HDDEVSEL,
        0xA0 | (drive << 4)
    );

    ata_delay(channel);

    /*
     * Clear sector and LBA registers.
     */
    outb(base + ATA_REG_SECCOUNT0, 0);
    outb(base + ATA_REG_LBA0, 0);
    outb(base + ATA_REG_LBA1, 0);
    outb(base + ATA_REG_LBA2, 0);

    /*
     * Request IDENTIFY.
     */
    outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(base + ATA_REG_STATUS);

    if (status == 0 || status == 0xFF)
        return -1;

    /*
     * Wait for the drive to finish processing.
     */
    if (ata_wait_not_busy(channel) != 0)
        return -1;

    /*
     * ATAPI devices use a different identification protocol.
     * This initial driver supports ATA hard drives only.
     */
    uint8_t mid = inb(base + ATA_REG_LBA1);
    uint8_t high = inb(base + ATA_REG_LBA2);

    if (mid != 0 || high != 0)
        return -1;

    /*
     * Wait for identification data.
     */
    if (ata_wait_drq(channel) != 0)
        return -1;

    /*
     * Read 256 words (512 bytes).
     */
    for (uint32_t i = 0; i < 256; i++)
    {
        identify_data[i] = ata_inw(base + ATA_REG_DATA);
    }

    device->present = 1;
    device->channel = channel_index;
    device->drive = drive;
    device->type = ATA_DEVICE_ATA;

    device->signature = identify_data[0];
    device->capabilities = identify_data[49];

    /*
     * LBA28 capacity.
     */
    device->size =
        ((uint32_t)identify_data[61] << 16) |
        identify_data[60];

    /*
     * Supported command sets.
     */
    device->command_sets =
        ((uint32_t)identify_data[83] << 16) |
        identify_data[82];

    /*
     * Read model name.
     */
    ata_read_string(
        &identify_data[27],
        device->model,
        20
    );

    return 0;
}


/*
 * Initialize the ATA subsystem.
 */
void ata_initialize(void)
{
    terminal_write("[ATA] Initializing storage subsystem...\n");

    channels[0].base = ATA_PRIMARY_IO;
    channels[0].ctrl = ATA_PRIMARY_CTRL;

    channels[1].base = ATA_SECONDARY_IO;
    channels[1].ctrl = ATA_SECONDARY_CTRL;

    device_count = 0;

    for (uint8_t channel = 0; channel < 2; channel++)
    {
        for (uint8_t drive = 0; drive < 2; drive++)
        {
            if (device_count >= ATA_MAX_DEVICES)
                break;

            ata_device_t device = {0};

            if (ata_identify(channel, drive, &device) == 0)
            {
                devices[device_count] = device;
                device_count++;

                terminal_write("[ATA] ATA hard drive detected: ");
                terminal_write(device.model);
                terminal_write("\n");
            }
        }
    }

    if (device_count == 0)
    {
        terminal_write("[ATA] No supported ATA hard drives detected.\n");
    }
    else
    {
        terminal_write("[ATA] Storage initialization complete.\n");
    }
}


/*
 * Return the number of detected ATA devices.
 */
uint8_t ata_get_device_count(void)
{
    return device_count;
}


/*
 * Retrieve a device.
 */
const ata_device_t* ata_get_device(uint8_t index)
{
    if (index >= device_count)
        return 0;

    return &devices[index];
}

int ata_read_sector(
    uint8_t device_index,
    uint32_t lba,
    uint16_t* buffer
)
{
    if (device_index >= device_count || buffer == 0)
        return -1;

    ata_device_t* device = &devices[device_index];

    if (!device->present || device->type != ATA_DEVICE_ATA)
        return -1;

    /*
     * Validate LBA28 addressing.
     */
    if (lba > 0x0FFFFFFF)
        return -1;

    if (lba >= device->size)
        return -1;

    ata_channel_t* channel = &channels[device->channel];

    uint16_t base = channel->base;

    /*
     * IMPORTANT:
     * Select the requested drive BEFORE checking its status.
     */
    outb(
        base + ATA_REG_HDDEVSEL,
        0xE0 |
        (device->drive << 4) |
        ((lba >> 24) & 0x0F)
    );

    ata_delay(channel);

    /*
     * Now check the selected drive.
     */
    if (ata_wait_not_busy(channel) != 0)
        return -1;

    /*
     * Configure one-sector read.
     */
    outb(base + ATA_REG_SECCOUNT0, 1);

    outb(base + ATA_REG_LBA0, lba & 0xFF);
    outb(base + ATA_REG_LBA1, (lba >> 8) & 0xFF);
    outb(base + ATA_REG_LBA2, (lba >> 16) & 0xFF);

    /*
     * Issue READ SECTORS command.
     */
    outb(base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    /*
     * Wait for the drive to provide data.
     */
    if (ata_wait_drq(channel) != 0)
        return -1;

    /*
     * Read 256 words (512 bytes).
     */
    for (uint32_t i = 0; i < 256; i++)
    {
        buffer[i] = ata_inw(base + ATA_REG_DATA);
    }

    /*
     * Confirm the command completed.
     */
    if (ata_wait_not_busy(channel) != 0)
        return -1;

    return 0;
}
