\
/*
 * ESP32-S3 LittleFS Demo (ESP-IDF)
 *
 * Demonstrates how to mount LittleFS, write/append/read files, list directory
 * contents, and query filesystem usage.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"

static const char *TAG = "littlefs_demo";

/**
 * @brief Mount LittleFS filesystem 
 * 
 * @return esp_err_t ESP_OK on success, otherwise failure code 
 */
static esp_err_t littlefs_mount(void) 
{
    // Configuration for LittleFS
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Register and mount LittleFS
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_vfs_littlefs_register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;

    // Query LittleFS usage
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted at %s", conf.base_path);
        ESP_LOGI(TAG, "Partition: total=%u bytes, used=%u bytes", (unsigned)total, (unsigned)used);
    } else {
        ESP_LOGW(TAG, "Mounted, but failed to query info: %s", esp_err_to_name(ret));
    }

    return ESP_OK;
}

/**
 * @brief Unmount LittleFS filesystem 
 * 
 */
static void littlefs_unmount(void)
{
    // Unregister and unmount LittleFS
    esp_vfs_littlefs_unregister("littlefs");
    ESP_LOGI(TAG, "LittleFS unmounted");
}

/**
 * @brief Ensure a directory exists, create if needed 
 * 
 * @param path Path to directory 
 */
static void ensure_dir(const char *path)
{
    struct stat st;
    
    // Check if path exists
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return;
        }
        ESP_LOGW(TAG, "Path exists but is not a directory: %s", path);
        return;
    }

    // Create directory
    if (mkdir(path, 0775) == 0) {
        ESP_LOGI(TAG, "Created directory: %s", path);
        return;
    }

    ESP_LOGE(TAG, "mkdir failed for %s: errno=%d (%s)", path, errno, strerror(errno));
}

/**
 * @brief Write text to a file (overwrites existing content) 
 * 
 * @param path Path to text file to write 
 * @param text Text to write 
 */
static void write_text_file(const char *path, const char *text)
{
    // Open file for writing (overwrite)
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "fopen(w) failed for %s: errno=%d (%s)", path, errno, strerror(errno));
        return;
    }

    // Write text to file
    size_t n = fwrite(text, 1, strlen(text), f);
    
    // Close file
    fclose(f);

    ESP_LOGI(TAG, "Wrote %u bytes to %s", (unsigned)n, path);
}

/**
 * @brief Append text to a file 
 * 
 * @param path Path to text file to append to 
 * @param text Text to append 
 */
static void append_text_file(const char *path, const char *text)
{
    // Open file for appending
    FILE *f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG, "fopen(a) failed for %s: errno=%d (%s)", path, errno, strerror(errno));
        return;
    }

    // Append text to file
    size_t n = fwrite(text, 1, strlen(text), f);
    
    // Close file
    fclose(f);

    ESP_LOGI(TAG, "Appended %u bytes to %s", (unsigned)n, path);
}

/**
 * @brief Read and log contents of a text file 
 * 
 * @param path Path to text file to read 
 */
static void read_text_file(const char *path)
{
    // Open file for reading
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "fopen(r) failed for %s: errno=%d (%s)", path, errno, strerror(errno));
        return;
    }

    ESP_LOGI(TAG, "---- Begin file: %s ----", path);

    // Read and log lines
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL) {
        // Lines already include '\n' typically
        ESP_LOGI(TAG, "%s", line);
    }

    // Close file
    fclose(f);
    ESP_LOGI(TAG, "---- End file ----");
}

/**
 * @brief List directory contents   
 * 
 * @param dirpath Path to directory to list 
 */
static void list_dir(const char *dirpath)
{
    // Open directory
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "opendir failed for %s: errno=%d (%s)", dirpath, errno, strerror(errno));
        return;
    }

    ESP_LOGI(TAG, "Directory listing for: %s", dirpath);

    // Read entries
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char fullpath[256];
        int written = snprintf(
            fullpath,
            sizeof(fullpath),
            "%s/%s",
            dirpath,
            ent->d_name
        );

        if (written < 0 || written >= (int)sizeof(fullpath)) {
            ESP_LOGW(TAG, "Path truncated, skipping: %s/%s", dirpath, ent->d_name);
            continue;
        }

        struct stat st;
        if (stat(fullpath, &st) == 0) {
            const char *type = S_ISDIR(st.st_mode) ? "DIR " : "FILE";
            ESP_LOGI(TAG, "  %s  %s  size=%u", type, ent->d_name, (unsigned)st.st_size);
        } else {
            ESP_LOGI(TAG, "  ?    %s", ent->d_name);
        }
    }

    // Close directory
    closedir(dir);
}

/**
 * @brief Show filesystem information
 * 
 */
static void show_fs_info(void)
{
    size_t total = 0;
    size_t used = 0;

    // Query LittleFS usage
    esp_err_t ret = esp_littlefs_info("littlefs", &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_littlefs_info failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "LittleFS usage: used=%u / total=%u bytes (free=%u bytes)",
             (unsigned)used, (unsigned)total, (unsigned)(total - used));
}

/**
 * @brief Application main entry point
 * 
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Booting LittleFS demo...");

    // Mount LittleFS
    esp_err_t ret = littlefs_mount();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Mount failed, stopping demo.");
        return;
    }

    // Prepare directories and files
    const char *base_dir = "/littlefs";
    const char *cfg_dir  = "/littlefs/config";
    const char *log_dir  = "/littlefs/logs";

    // Ensure directories exist
    ensure_dir(cfg_dir);
    ensure_dir(log_dir);

    // File paths
    const char *cfg_path = "/littlefs/config/device.cfg";
    const char *log_path = "/littlefs/logs/boot.log";

    // Write initial config file
    write_text_file(cfg_path,
                    "device_id=ESP32S3\n"
                    "mode=demo\n"
                    "wifi_autostart=false\n");

    // Append to config file
    append_text_file(cfg_path, "log_enabled=true\n");

    // Write initial log file
    append_text_file(log_path, "boot=ok\n");

    // Read back files
    read_text_file(cfg_path);

    // List directories
    list_dir(base_dir);
    list_dir(cfg_dir);
    list_dir(log_dir);

    // Show filesystem usage
    show_fs_info();

    // Keep running to allow monitor viewing; periodically append to log
    for (int i = 0; i < 5; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "tick=%d\n", i);
        append_text_file(log_path, buf);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    // Final read of log file
    read_text_file(log_path);
    
    // Final filesystem usage
    show_fs_info();

    // Optional unmount (usually not required in embedded apps)
    littlefs_unmount();

    ESP_LOGI(TAG, "Demo complete.");
}