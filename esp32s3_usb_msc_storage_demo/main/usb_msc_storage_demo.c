/**
 * @file usb_msc_storage_demo.c
 * @brief USB Mass Storage Class device backed by an internal FAT partition.
 *
 * This example turns the ESP32-S3 into a USB flash drive. The project uses the
 * native USB-OTG peripheral, the Espressif TinyUSB component, a FAT file system,
 * and wear levelling over a dedicated internal flash partition.
 *
 * The firmware first gives the application exclusive ownership of the volume,
 * creates educational text files, and then transfers exclusive ownership to the
 * USB host. The application never accesses the FAT volume while the host owns it.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "tusb.h"
#include "wear_levelling.h"

#define STORAGE_BASE_PATH "/usb"
#define STORAGE_PARTITION_LABEL "usb_storage"
#define STORAGE_MAX_OPEN_FILES 4

#define USB_VID 0x303A
#define USB_PID 0x4013
#define USB_BCD_DEVICE 0x0100

#define MSC_OUT_ENDPOINT 0x01
#define MSC_IN_ENDPOINT  0x81
#define USB_CONFIGURATION_TOTAL_LENGTH (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN)

static const char *TAG = "usb_msc_demo";

static tinyusb_msc_storage_handle_t s_storage_handle = NULL;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

enum {
    USB_INTERFACE_MSC = 0,
    USB_INTERFACE_COUNT,
};

/** Device descriptor */
static const tusb_desc_device_t s_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = USB_BCD_DEVICE,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

/** Full-speed configuration descriptor */
static const uint8_t s_full_speed_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1,
        USB_INTERFACE_COUNT,
        0,
        USB_CONFIGURATION_TOTAL_LENGTH,
        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
        100),
    TUD_MSC_DESCRIPTOR(
        USB_INTERFACE_MSC,
        4,
        MSC_OUT_ENDPOINT,
        MSC_IN_ENDPOINT,
        64),
};

/** String descriptors */
static const char *s_string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Yamil Embedded",
    "ESP32-S3 USB Storage Lab",
    "MSC-PART13-001",
    "Mass Storage Interface",
};

 /** @brief Initializes the flash storage for USB MSC operation.
  *
  * @param wl_handle Output pointer that receives the wear-levelling handle.
  *
  * @return ESP_OK when the partition was found and mounted, or an ESP-IDF error code.
  */
static esp_err_t initialize_flash_storage(wl_handle_t *wl_handle)
{
    if (wl_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_FAT,
        STORAGE_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(TAG, "FAT partition '%s' was not found", STORAGE_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG,
             "Using partition '%s': offset=0x%" PRIx32 ", size=%" PRIu32 " bytes",
             partition->label,
             partition->address,
             partition->size);

    return wl_mount(partition, wl_handle);
}

/** @brief Writes text to a file on the application-owned FAT volume.
 *
 * @param path Path to the file to be created or overwritten.
 * @param contents Null-terminated string to be written to the file.
 * 
 * @return ESP_OK when the file is written successfully, or an error code.
 */
static esp_err_t write_text_file(const char *path, const char *contents)
{
    if ((path == NULL) || (contents == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *file = fopen(path, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Unable to open %s for writing", path);
        return ESP_FAIL;
    }

    const size_t text_length = strlen(contents);
    const size_t written = fwrite(contents, 1, text_length, file);
    if (written != text_length) {
        ESP_LOGE(TAG, "Incomplete write to %s", path);
        fclose(file);
        return ESP_FAIL;
    }

    // fflush() pushes stdio data into the FAT driver before ownership changes.
    if (fflush(file) != 0) {
        ESP_LOGE(TAG, "Unable to flush %s", path);
        fclose(file);
        return ESP_FAIL;
    }

    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "Unable to close %s", path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/** @brief Creates the initial educational files stored on the USB volume.
 *
 * The files explain the relationship between MSC, SCSI, logical blocks, and FAT.
 * They are created only while the application has exclusive ownership.
 *
 * @return ESP_OK when all files are written successfully, or an error code.
 */
static esp_err_t create_demo_files(void)
{
    static const char readme_text[] =
        "ESP32-S3 USB MASS STORAGE LAB\r\n"
        "================================\r\n\r\n"
        "This drive is provided by the ESP32-S3 native USB peripheral.\r\n"
        "The USB host communicates with the device through Mass Storage Class\r\n"
        "Bulk-Only Transport and SCSI commands. The host reads and writes 512-byte\r\n"
        "logical blocks. FAT converts those blocks into files and directories.\r\n\r\n"
        "Important: Always eject the drive before disconnecting USB power.\r\n";

    static const char layers_text[] =
        "APPLICATION FILE OPERATIONS\r\n"
        "          |\r\n"
        "       FAT FILE SYSTEM\r\n"
        "          |\r\n"
        "   512-BYTE LOGICAL BLOCKS\r\n"
        "          |\r\n"
        "       SCSI COMMANDS\r\n"
        "          |\r\n"
        " USB MSC BULK-ONLY TRANSPORT\r\n"
        "          |\r\n"
        "      USB BULK ENDPOINTS\r\n"
        "          |\r\n"
        " ESP32-S3 USB-OTG PERIPHERAL\r\n";

    static const char scsi_text[] =
        "COMMON SCSI COMMANDS\r\n"
        "====================\r\n"
        "INQUIRY          - Identifies the storage device.\r\n"
        "TEST UNIT READY  - Checks whether the medium is available.\r\n"
        "READ CAPACITY    - Reports the last LBA and logical block size.\r\n"
        "READ(10)         - Reads one or more logical blocks.\r\n"
        "WRITE(10)        - Writes one or more logical blocks.\r\n"
        "REQUEST SENSE    - Returns detailed error information.\r\n"
        "SYNCHRONIZE CACHE- Requests pending writes to be committed.\r\n";

    ESP_RETURN_ON_ERROR(
        write_text_file(STORAGE_BASE_PATH "/README.TXT", readme_text),
        TAG,
        "Failed to create README.TXT");
    ESP_RETURN_ON_ERROR(
        write_text_file(STORAGE_BASE_PATH "/LAYERS.TXT", layers_text),
        TAG,
        "Failed to create LAYERS.TXT");
    ESP_RETURN_ON_ERROR(
        write_text_file(STORAGE_BASE_PATH "/SCSI.TXT", scsi_text),
        TAG,
        "Failed to create SCSI.TXT");

    return ESP_OK;
}

/**
 * @brief Reports storage ownership transitions generated by the MSC component.
 *
 * @param handle Storage object associated with the event.
 * @param event Event information including the destination mount point.
 * @param arg Optional user argument. This example does not use it.
 */
static void storage_mount_changed_callback(tinyusb_msc_storage_handle_t handle,
                                           tinyusb_msc_event_t *event,
                                           void *arg)
{
    (void)handle;
    (void)arg;

    if (event == NULL) {
        return;
    }

    switch (event->id) {
    case TINYUSB_MSC_EVENT_MOUNT_START:
        ESP_LOGI(TAG, "Storage ownership transition started");
        break;

    case TINYUSB_MSC_EVENT_MOUNT_COMPLETE:
        ESP_LOGI(TAG,
                 "Storage owner: %s",
                 (event->mount_point == TINYUSB_MSC_STORAGE_MOUNT_USB)
                     ? "USB host"
                     : "ESP32-S3 application");
        break;

    case TINYUSB_MSC_EVENT_MOUNT_FAILED:
        ESP_LOGE(TAG, "Storage ownership transition failed");
        break;

    case TINYUSB_MSC_EVENT_FORMAT_REQUIRED:
        ESP_LOGW(TAG, "Storage required FAT formatting");
        break;

    default:
        ESP_LOGD(TAG, "Unhandled storage event: %d", event->id);
        break;
    }
}

/** @brief Initializes the MSC storage adapter.
 *
 * @return esp_err_t
 */
static esp_err_t initialize_msc_storage(void)
{
    ESP_RETURN_ON_ERROR(initialize_flash_storage(&s_wl_handle), TAG, "Flash storage initialization failed");

    tinyusb_msc_storage_config_t storage_config = {
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .medium.wl_handle = s_wl_handle,
        .fat_fs = {
            .base_path = STORAGE_BASE_PATH,
            .config = {
                .format_if_mount_failed = true,
                .max_files = STORAGE_MAX_OPEN_FILES,
                .allocation_unit_size = 4096,
            },
            .format_flags = 0,
        },
    };

    ESP_RETURN_ON_ERROR(
        tinyusb_msc_new_storage_spiflash(&storage_config, &s_storage_handle),
        TAG,
        "Unable to create the MSC storage adapter");

    ESP_RETURN_ON_ERROR(
        tinyusb_msc_set_storage_callback(storage_mount_changed_callback, NULL),
        TAG,
        "Unable to register the storage event callback");

    // Give local code exclusive ownership before creating the example files.
    ESP_RETURN_ON_ERROR(
        tinyusb_msc_set_storage_mount_point(
            s_storage_handle,
            TINYUSB_MSC_STORAGE_MOUNT_APP),
        TAG,
        "Unable to mount the FAT volume for the application");

    return ESP_OK;
}


/**
 * @brief Installs the TinyUSB device stack with an MSC-only USB configuration. 
 * 
 * @return esp_err_t 
 */
static esp_err_t initialize_usb_device(void)
{
    tinyusb_config_t usb_config = TINYUSB_DEFAULT_CONFIG();
    usb_config.descriptor.device = &s_device_descriptor;
    usb_config.descriptor.full_speed_config = s_full_speed_configuration_descriptor;
    usb_config.descriptor.string = s_string_descriptors;
    usb_config.descriptor.string_count =
        sizeof(s_string_descriptors) / sizeof(s_string_descriptors[0]);

    return tinyusb_driver_install(&usb_config);
}

/**
 * @brief Logs the logical geometry advertised by the MSC storage adapter.
 * 
 * @return void 
 */
static void log_storage_geometry(void)
{
    uint32_t sector_size = 0;
    uint32_t sector_count = 0;

    if ((tinyusb_msc_get_storage_sector_size(s_storage_handle, &sector_size) == ESP_OK) &&
        (tinyusb_msc_get_storage_capacity(s_storage_handle, &sector_count) == ESP_OK)) {
        const uint64_t capacity = (uint64_t)sector_size * sector_count;
        ESP_LOGI(TAG, "Logical block size: %" PRIu32 " bytes", sector_size);
        ESP_LOGI(TAG, "Logical block count: %" PRIu32, sector_count);
        ESP_LOGI(TAG, "Reported capacity: %" PRIu64 " bytes", capacity);
    } else {
        ESP_LOGW(TAG, "Unable to query storage geometry");
    }
}

/** @brief Main application entry point.
 *
 * Initializes the flash-backed FAT volume, creates educational files, installs
 * TinyUSB, and transfers the volume to the connected USB host.
 * 
 * @return void 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 USB MSC storage demonstration");

    // The storage must be initialized before the USB device stack because the MSC interface 
    // is part of the USB configuration descriptors.
    ESP_ERROR_CHECK(initialize_msc_storage());
    
    // The example files are created before the USB host can access the volume to avoid conflicts.
    ESP_ERROR_CHECK(create_demo_files());
    
    // Log the geometry for educational purposes, even though the application doesn't use it directly.
    log_storage_geometry();

    // The USB device stack is installed after the storage is ready to ensure the host sees the initial files.
    ESP_ERROR_CHECK(initialize_usb_device());

    // The local FAT mount is cleanly removed before the computer gets access.    
    // After this call, the application must not access the FAT volume until the host ejects it and gives ownership back.
    ESP_ERROR_CHECK(tinyusb_msc_set_storage_mount_point(
        s_storage_handle,
        TINYUSB_MSC_STORAGE_MOUNT_USB));

    ESP_LOGI(TAG, "USB Mass Storage device is ready");
    ESP_LOGI(TAG, "Connect the native USB port and look for 'ESP32-S3 USB Storage Lab'");
    ESP_LOGI(TAG, "Safely eject the volume before unplugging it");
}
