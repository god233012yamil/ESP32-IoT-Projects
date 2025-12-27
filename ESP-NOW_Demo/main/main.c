/**
 * @file main.c
 * @brief ESP-NOW beginner demo for ESP32-S3 using ESP-IDF + FreeRTOS.
 *
 * This project provides two firmware roles:
 * - Sender: periodically transmits a small counter packet using ESP-NOW.
 * - Receiver: receives packets and prints them from a FreeRTOS task.
 *
 * Configure the role and parameters using:
 *   idf.py menuconfig
 *
 * Key beginner concepts demonstrated:
 * - Wi-Fi initialization (STA mode) without connecting to an AP
 * - ESP-NOW initialization and peer addressing
 * - Callback-to-task handoff using a FreeRTOS queue
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_now.h"

static const char *TAG = "espnow_demo";

#define RX_QUEUE_LEN            16
#define SEND_DONE_BIT           (1U << 0)

/** Simple application packet.
 *
 * Keep packets small and versioned. For real products, include:
 * - protocol version
 * - message type
 * - CRC or authentication tag (if needed)
 */
typedef struct {
    uint8_t  version;
    uint8_t  msg_type;
    uint16_t seq;
    uint32_t counter;
} app_packet_t;

typedef struct {
    uint8_t src_mac[6];
    int     len;
    app_packet_t pkt;
} rx_item_t;

static QueueHandle_t s_rx_queue = NULL;
static EventGroupHandle_t s_evt = NULL;

static uint8_t s_peer_mac[6] = {0};
static uint16_t s_seq = 0;

/**
 * @brief Convert a MAC string (AA:BB:CC:DD:EE:FF) into 6 bytes.
 *
 * Args:
 *   mac_str: Null-terminated string in the format "AA:BB:CC:DD:EE:FF".
 *   out_mac: Output buffer of size 6 bytes.
 *
 * Returns:
 *   true if parsing succeeded, false otherwise.
 */
static bool parse_mac_str(const char *mac_str, uint8_t out_mac[6])
{
    if (!mac_str || !out_mac) {
        return false;
    }

    unsigned int b[6] = {0};
    int n = sscanf(mac_str, "%2x:%2x:%2x:%2x:%2x:%2x",
                   &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    if (n != 6) {
        return false;
    }

    for (int i = 0; i < 6; i++) {
        out_mac[i] = (uint8_t)b[i];
    }
    return true;
}

/**
 * @brief Print a MAC address to a short string buffer.
 *
 * Args:
 *   mac: 6-byte MAC address.
 *   out: Output string buffer.
 *   out_len: Output buffer length (recommended >= 18).
 *
 * Returns:
 *   None.
 */
static void mac_to_str(const uint8_t mac[6], char *out, size_t out_len)
{
    if (!mac || !out || out_len < 18) {
        return;
    }
    snprintf(out, out_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief Initialize NVS (required by Wi-Fi in many ESP-IDF setups).
 *
 * Args:
 *   None.
 *
 * Returns:
 *   esp_err_t: ESP_OK on success, otherwise an error code.
 */
static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

/**
 * @brief Initialize Wi-Fi in STA mode for ESP-NOW operation.
 *
 * This does not connect to an AP. It only starts the Wi-Fi driver and sets
 * the channel used by ESP-NOW. Both devices must share the same channel.
 *
 * Args:
 *   channel: Wi-Fi channel to use (1-13 typically).
 *
 * Returns:
 *   esp_err_t: ESP_OK on success, otherwise an error code.
 */
static esp_err_t init_wifi_for_espnow(uint8_t channel)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    return ESP_OK;
}

/**
 * @brief ESP-NOW send callback.
 *
 * This callback is invoked by the Wi-Fi task when a send attempt completes.
 * For beginners: treat this as "radio delivery status", not a full application ack.
 *
 * Args:
 *   mac_addr: Destination MAC used in esp_now_send().
 *   status: ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL.
 *
 * Returns:
 *   None.
 */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    (void)mac_addr;

    // Signal send done event
    if (s_evt) {
        xEventGroupSetBits(s_evt, SEND_DONE_BIT);
    }

    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send status: SUCCESS");
    } else {
        ESP_LOGW(TAG, "Send status: FAIL");
    }
}

/**
 * @brief ESP-NOW receive callback.
 *
 * This callback is invoked by the Wi-Fi task when an ESP-NOW frame arrives.
 * Keep work minimal here. Copy and push into a queue for a normal task to handle.
 *
 * Args:
 *   info: Metadata about the received frame (includes source MAC).
 *   data: Pointer to received payload.
 *   len: Payload length in bytes.
 *
 * Returns:
 *   None.
 */
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (!info || !data || len <= 0) {
        return;
    }

    // Create RX item
    rx_item_t item = {0};

    // Copy payload source MAC
    memcpy(item.src_mac, info->src_addr, 6);

    // Copy payload length
    item.len = len;

    // Copy payload (ensure it fits)
    if ((size_t)len >= sizeof(app_packet_t)) {
        memcpy(&item.pkt, data, sizeof(app_packet_t));
    } else {
        return;
    }

    // Enqueue item for processing in a task
    if (s_rx_queue) {
        (void)xQueueSend(s_rx_queue, &item, 0);
    }
}

/**
 * @brief Add a peer (or prepare broadcast) for ESP-NOW sending.
 *
 * If the peer MAC is FF:FF:FF:FF:FF:FF, we treat it as broadcast and do not
 * add a peer entry (broadcast doesn't require pairing).
 *
 * Args:
 *   peer_mac: Destination MAC.
 *   channel: Wi-Fi channel used for ESP-NOW.
 *
 * Returns:
 *   esp_err_t: ESP_OK on success, otherwise an error code.
 */
static esp_err_t espnow_config_peer(const uint8_t peer_mac[6], uint8_t channel)
{
    static const uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, 6);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = channel;
    peer.encrypt = false;

    /* ESP-IDF requires the destination to exist in the peer list, even for broadcast,
     * otherwise esp_now_send() returns ESP_ERR_ESPNOW_NOT_FOUND.
     */
    if (esp_now_is_peer_exist(peer.peer_addr)) {
        if (memcmp(peer_mac, broadcast_mac, 6) == 0) {
            ESP_LOGI(TAG, "Broadcast peer already exists");
        } else {
            ESP_LOGI(TAG, "Peer already exists");
        }
        return ESP_OK;
    }

    if (memcmp(peer_mac, broadcast_mac, 6) == 0) {
        ESP_LOGI(TAG, "Adding broadcast peer (required for broadcast sends)");
    }

    return esp_now_add_peer(&peer);
}

/**
 * @brief Initialize ESP-NOW and register callbacks.
 *
 * Args:
 *   None.
 *
 * Returns:
 *   esp_err_t: ESP_OK on success, otherwise an error code.
 */
static esp_err_t init_espnow(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    return ESP_OK;
}

/**
 * @brief Sender task: transmit a small packet once per second.
 *
 * This task demonstrates:
 * - building a small fixed-size payload
 * - calling esp_now_send()
 * - optional wait on a send-done event bit
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None.
 */
static void sender_task(void *arg)
{
    (void)arg;

    uint32_t counter = 0;

    while (1) {
        // Build packet
        app_packet_t pkt = {0};
        pkt.version = 1;
        pkt.msg_type = 1;
        pkt.seq = s_seq++;
        pkt.counter = counter++;

        // Send packet via ESP-NOW
        esp_err_t err = esp_now_send(s_peer_mac, (const uint8_t *)&pkt, sizeof(pkt));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
        }

        // Wait for send to complete (optional)
        if (s_evt) {
            (void)xEventGroupWaitBits(s_evt, SEND_DONE_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(200));
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Receiver task: print packets forwarded from the receive callback.
 *
 * This task demonstrates:
 * - receiving items from a FreeRTOS queue
 * - safely processing data outside the ESP-NOW callback context
 *
 * Args:
 *   arg: Unused.
 *
 * Returns:
 *   None.
 */
static void receiver_task(void *arg)
{
    (void)arg;

    rx_item_t item;

    while (1) {
        // Wait indefinitely for a received item
        if (xQueueReceive(s_rx_queue, &item, portMAX_DELAY) == pdTRUE) {

            char mac_str[18] = {0};

            // Convert source MAC to string
            mac_to_str(item.src_mac, mac_str, sizeof(mac_str));

            // Print received packet details
            ESP_LOGI(TAG, "RX from %s | ver=%u type=%u seq=%u counter=%" PRIu32 " (len=%d)",
                     mac_str,
                     item.pkt.version,
                     item.pkt.msg_type,
                     item.pkt.seq,
                     item.pkt.counter,
                     item.len);
        }
    }
}

/**
 * @brief Application entry point.
 *
 * This function:
 * - initializes NVS and Wi-Fi
 * - initializes ESP-NOW and configures peer/broadcast
 * - starts the sender or receiver task depending on menuconfig
 *
 * Args:
 *   None.
 *
 * Returns:
 *   None.
 */
void app_main(void)
{
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());

    // Initialize Wi-Fi for ESP-NOW
    uint8_t channel = (uint8_t)CONFIG_ESPNOW_CHANNEL;
    ESP_ERROR_CHECK(init_wifi_for_espnow(channel));

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(init_espnow());

    // Parse peer MAC from menuconfig
    if (!parse_mac_str(CONFIG_ESPNOW_PEER_MAC, s_peer_mac)) {
        ESP_LOGE(TAG, "Invalid peer MAC string: '%s'", CONFIG_ESPNOW_PEER_MAC);
        ESP_LOGE(TAG, "Expected format: AA:BB:CC:DD:EE:FF");
        return;
    }

    // Convert and log peer MAC
    char peer_str[18] = {0};
    mac_to_str(s_peer_mac, peer_str, sizeof(peer_str));
    ESP_LOGI(TAG, "Configured channel=%u peer=%s", channel, peer_str);

    // Configure peer for ESP-NOW
    ESP_ERROR_CHECK(espnow_config_peer(s_peer_mac, channel));

    // Create RX queue and event group
    s_rx_queue = xQueueCreate(RX_QUEUE_LEN, sizeof(rx_item_t));
    if (!s_rx_queue) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return;
    }

    // Create event group
    s_evt = xEventGroupCreate();
    if (!s_evt) {
        ESP_LOGE(TAG, "Failed to create event group");
        return;
    }

    // Start sender or receiver task based on menuconfig
#if CONFIG_ESPNOW_ROLE_SENDER
    ESP_LOGI(TAG, "Role: SENDER");
    xTaskCreate(sender_task, "sender_task", 4096, NULL, 5, NULL);
#else
    ESP_LOGI(TAG, "Role: RECEIVER");
    xTaskCreate(receiver_task, "receiver_task", 4096, NULL, 5, NULL);
#endif
}