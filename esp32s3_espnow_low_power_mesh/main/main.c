#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "mesh_protocol.h"
#include "power_manager.h"

#define RX_QUEUE_LENGTH 12U
#define RX_EVENT_PACKET BIT0
#define RX_EVENT_BEACON BIT1
#define RX_EVENT_ACK BIT2
#define MAX_RECENT_SEQUENCES 16U

static const char *TAG = "mesh_app";
static const uint8_t BROADCAST_MAC[6] = {
    0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU
};

typedef struct {
    uint8_t source_mac[6];
    mesh_packet_t packet;
} rx_item_t;

typedef struct {
    uint16_t source_id;
    uint16_t sequence;
} sequence_record_t;

RTC_DATA_ATTR static uint16_t s_sequence = 0U;
RTC_DATA_ATTR static uint32_t s_boot_count = 0U;

static QueueHandle_t s_rx_queue;
static EventGroupHandle_t s_events;
static uint8_t s_parent_mac[6];
static uint16_t s_last_acked_sequence;
static sequence_record_t s_recent_sequences[MAX_RECENT_SEQUENCES];
static size_t s_recent_index;

/**
 * @brief Returns the configured logical node role name.
 *
 * Returns:
 *     Constant role name string.
 */
static const char *node_role_name(void)
{
#if CONFIG_APP_ROLE_ROOT
    return "root";
#elif CONFIG_APP_ROLE_ROUTER
    return "router";
#else
    return "leaf";
#endif
}

/**
 * @brief Initializes NVS and erases incompatible data when required.
 *
 * Returns:
 *     ESP_OK on success or an NVS error code.
 */
static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if ((err == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (err == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    return err;
}

/**
 * @brief Initializes Wi-Fi in station mode on a fixed ESP-NOW channel.
 *
 * Returns:
 *     ESP_OK on success or a Wi-Fi initialization error code.
 */
static esp_err_t initialize_wifi(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG,
                        "event loop creation failed");

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG,
                        "esp_wifi_set_storage failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG,
                        "esp_wifi_set_mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start failed");

    // Lock every node to the same channel because ESP-NOW does not roam.
    ESP_RETURN_ON_ERROR(
        esp_wifi_set_channel(CONFIG_APP_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE),
        TAG,
        "esp_wifi_set_channel failed");

    return ESP_OK;
}

/**
 * @brief Adds or updates an ESP-NOW peer.
 *
 * Args:
 *     mac: Peer MAC address.
 *     encrypted: True to enable peer encryption.
 *
 * Returns:
 *     ESP_OK on success or an ESP-NOW error code.
 */
static esp_err_t add_peer(const uint8_t mac[6], bool encrypted)
{
    if (esp_now_is_peer_exist(mac)) {
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, mac, sizeof(peer.peer_addr));
    peer.channel = CONFIG_APP_WIFI_CHANNEL;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = encrypted;

#if CONFIG_APP_ENABLE_ENCRYPTION
    if (encrypted) {
        memcpy(peer.lmk, CONFIG_APP_LMK, ESP_NOW_KEY_LEN);
    }
#else
    (void)encrypted;
#endif

    return esp_now_add_peer(&peer);
}

/**
 * @brief ESP-NOW send completion callback.
 *
 * Args:
 *     info: Transmission metadata supplied by ESP-IDF.
 *     status: Delivery status reported by the Wi-Fi driver.
 */
static void espnow_send_callback(const uint8_t *destination_mac,
                                 esp_now_send_status_t status)
{
    if (destination_mac == NULL) {
        return;
    }

    ESP_LOGD(TAG,
             "TX to %02X:%02X:%02X:%02X:%02X:%02X status=%s",
             destination_mac[0], destination_mac[1], destination_mac[2],
             destination_mac[3], destination_mac[4], destination_mac[5],
             (status == ESP_NOW_SEND_SUCCESS) ? "ok" : "failed");
}

/**
 * @brief ESP-NOW receive callback that copies packets into a FreeRTOS queue.
 *
 * Args:
 *     info: Reception metadata containing source and destination MAC addresses.
 *     data: Received packet bytes.
 *     length: Number of received bytes.
 */
static void espnow_receive_callback(const esp_now_recv_info_t *info,
                                    const uint8_t *data,
                                    int length)
{
    if ((info == NULL) || (data == NULL) ||
        (length != (int)sizeof(mesh_packet_t))) {
        return;
    }

    if (mesh_packet_validate(data, (size_t)length) != ESP_OK) {
        return;
    }

    rx_item_t item = {0};
    memcpy(item.source_mac, info->src_addr, sizeof(item.source_mac));
    memcpy(&item.packet, data, sizeof(item.packet));

    // The callback runs in the Wi-Fi task, so never block here.
    if (xQueueSend(s_rx_queue, &item, 0) == pdTRUE) {
        xEventGroupSetBits(s_events, RX_EVENT_PACKET);
    }
}

/**
 * @brief Initializes ESP-NOW, callbacks, and required peers.
 *
 * Returns:
 *     ESP_OK on success or an ESP-NOW initialization error code.
 */
static esp_err_t initialize_espnow(void)
{
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init failed");
    ESP_RETURN_ON_ERROR(esp_now_register_send_cb(espnow_send_callback), TAG,
                        "send callback registration failed");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(espnow_receive_callback), TAG,
                        "receive callback registration failed");

#if CONFIG_APP_ENABLE_ENCRYPTION
    ESP_RETURN_ON_ERROR(esp_now_set_pmk((const uint8_t *)CONFIG_APP_PMK), TAG,
                        "esp_now_set_pmk failed");
#endif

    // Broadcast peer is required for synchronization beacons.
    ESP_RETURN_ON_ERROR(add_peer(BROADCAST_MAC, false), TAG,
                        "broadcast peer creation failed");

#if !CONFIG_APP_ROLE_ROOT
    ESP_RETURN_ON_ERROR(mesh_parse_mac(CONFIG_APP_PARENT_MAC, s_parent_mac), TAG,
                        "invalid parent MAC address");
#if CONFIG_APP_ENABLE_ENCRYPTION
    const bool parent_peer_encrypted = true;
#else
    const bool parent_peer_encrypted = false;
#endif
    ESP_RETURN_ON_ERROR(
        add_peer(s_parent_mac, parent_peer_encrypted),
        TAG,
        "parent peer creation failed");
#endif

    return ESP_OK;
}

/**
 * @brief Sends a finalized mesh packet to an ESP-NOW peer.
 *
 * Args:
 *     destination_mac: Destination peer MAC address.
 *     packet: Packet to transmit.
 *
 * Returns:
 *     ESP_OK on success or an ESP-NOW transmission error code.
 */
static esp_err_t send_packet(const uint8_t destination_mac[6],
                             mesh_packet_t *packet)
{
    mesh_packet_finalize(packet);
    return esp_now_send(destination_mac,
                        (const uint8_t *)packet,
                        sizeof(*packet));
}

/**
 * @brief Sends an application-level acknowledgement packet.
 *
 * Args:
 *     destination_mac: MAC address of the original sender.
 *     acknowledged_sequence: Sequence number being acknowledged.
 *
 * Returns:
 *     ESP_OK on success or an ESP-NOW error code.
 */
static esp_err_t send_ack(const uint8_t destination_mac[6],
                          uint16_t acknowledged_sequence)
{
    ESP_RETURN_ON_ERROR(add_peer(destination_mac, false), TAG,
                        "temporary ACK peer creation failed");

    mesh_packet_t ack = {
        .version = MESH_PROTOCOL_VERSION,
        .type = MESH_PACKET_ACK,
        .source_id = CONFIG_APP_NODE_ID,
        .destination_id = MESH_BROADCAST_NODE_ID,
        .sequence = acknowledged_sequence,
        .ttl = 1U,
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
    };

    return send_packet(destination_mac, &ack);
}

/**
 * @brief Checks and records recently received packet sequences.
 *
 * Args:
 *     source_id: Logical source node identifier.
 *     sequence: Packet sequence number.
 *
 * Returns:
 *     True when the packet is a duplicate; otherwise false.
 */
static bool is_duplicate_packet(uint16_t source_id, uint16_t sequence)
{
    for (size_t i = 0; i < MAX_RECENT_SEQUENCES; ++i) {
        if ((s_recent_sequences[i].source_id == source_id) &&
            (s_recent_sequences[i].sequence == sequence)) {
            return true;
        }
    }

    s_recent_sequences[s_recent_index].source_id = source_id;
    s_recent_sequences[s_recent_index].sequence = sequence;
    s_recent_index = (s_recent_index + 1U) % MAX_RECENT_SEQUENCES;
    return false;
}

/**
 * @brief Logs a received sensor packet at the root gateway.
 *
 * Args:
 *     packet: Sensor packet to display.
 *     source_mac: Immediate sender MAC address.
 */
static void log_sensor_packet(const mesh_packet_t *packet,
                              const uint8_t source_mac[6])
{
    ESP_LOGI(TAG,
             "Sensor src=%u seq=%u temp=%.2f C humidity=%.2f %% "
             "battery=%u mV ttl=%u via=%02X:%02X:%02X:%02X:%02X:%02X",
             packet->source_id,
             packet->sequence,
             packet->temperature_centi_c / 100.0,
             packet->humidity_centi_pct / 100.0,
             packet->battery_mv,
             packet->ttl,
             source_mac[0], source_mac[1], source_mac[2],
             source_mac[3], source_mac[4], source_mac[5]);
}

/**
 * @brief Processes one received packet according to the configured node role.
 *
 * Args:
 *     item: Queue item containing the source MAC and validated packet.
 */
static void process_received_packet(const rx_item_t *item)
{
    const mesh_packet_t *packet = &item->packet;

    if (packet->type == MESH_PACKET_BEACON) {
        xEventGroupSetBits(s_events, RX_EVENT_BEACON);
        return;
    }

    if (packet->type == MESH_PACKET_ACK) {
        s_last_acked_sequence = packet->sequence;
        xEventGroupSetBits(s_events, RX_EVENT_ACK);
        return;
    }

    if (packet->type != MESH_PACKET_SENSOR) {
        return;
    }

    // ACK the immediate sender even when the payload is a duplicate.
    ESP_ERROR_CHECK_WITHOUT_ABORT(send_ack(item->source_mac, packet->sequence));

    if (is_duplicate_packet(packet->source_id, packet->sequence)) {
        ESP_LOGD(TAG, "Duplicate packet src=%u seq=%u ignored",
                 packet->source_id, packet->sequence);
        return;
    }

#if CONFIG_APP_ROLE_ROOT
    log_sensor_packet(packet, item->source_mac);
#elif CONFIG_APP_ROLE_ROUTER
    if (packet->ttl <= 1U) {
        ESP_LOGW(TAG, "Dropping packet src=%u seq=%u because TTL expired",
                 packet->source_id, packet->sequence);
        return;
    }

    mesh_packet_t forwarded = *packet;
    forwarded.ttl--;
    ESP_ERROR_CHECK_WITHOUT_ABORT(send_packet(s_parent_mac, &forwarded));
    ESP_LOGI(TAG, "Forwarded src=%u seq=%u ttl=%u",
             forwarded.source_id, forwarded.sequence, forwarded.ttl);
#else
    // Leaf nodes normally transmit only, but still accept ACKs and beacons.
    ESP_LOGD(TAG, "Leaf ignored sensor packet from node %u",
             packet->source_id);
#endif
}

/**
 * @brief Drains the receive queue and processes all pending packets.
 */
static void drain_receive_queue(void)
{
    rx_item_t item;
    while (xQueueReceive(s_rx_queue, &item, 0) == pdTRUE) {
        process_received_packet(&item);
    }
}

/**
 * @brief Builds deterministic demo sensor values without external hardware.
 *
 * Args:
 *     packet: Packet whose sensor fields will be populated.
 */
static void populate_demo_sensor_data(mesh_packet_t *packet)
{
    const uint32_t sample = s_boot_count % 50U;

    // Replace these values with real I2C, SPI, ADC, or 1-Wire readings.
    packet->temperature_centi_c = (int16_t)(2200 + (int32_t)sample * 3);
    packet->humidity_centi_pct = (uint16_t)(4800 + sample * 5U);
    packet->battery_mv = (uint16_t)(4200U - (s_boot_count % 900U));
}

/**
 * @brief Sends one sensor packet and waits for an application ACK.
 *
 * Returns:
 *     ESP_OK when acknowledged; ESP_ERR_TIMEOUT when all retries fail.
 */
static esp_err_t transmit_sensor_packet_with_retry(void)
{
    mesh_packet_t packet = {
        .version = MESH_PROTOCOL_VERSION,
        .type = MESH_PACKET_SENSOR,
        .source_id = CONFIG_APP_NODE_ID,
        .destination_id = 0U,
        .sequence = ++s_sequence,
        .ttl = CONFIG_APP_FORWARD_TTL,
        .flags = 0U,
        .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
    };
    populate_demo_sensor_data(&packet);

    for (uint32_t attempt = 0U; attempt <= CONFIG_APP_MAX_RETRIES; ++attempt) {
        xEventGroupClearBits(s_events, RX_EVENT_ACK);
        s_last_acked_sequence = 0U;

        ESP_LOGI(TAG, "Sending sensor packet seq=%u attempt=%lu",
                 packet.sequence, (unsigned long)(attempt + 1U));
        ESP_RETURN_ON_ERROR(send_packet(s_parent_mac, &packet), TAG,
                            "sensor packet send failed");

        const TickType_t timeout_ticks = pdMS_TO_TICKS(CONFIG_APP_ACK_TIMEOUT_MS);
        const TickType_t start_tick = xTaskGetTickCount();

        while ((xTaskGetTickCount() - start_tick) < timeout_ticks) {
            EventBits_t bits = xEventGroupWaitBits(
                s_events,
                RX_EVENT_PACKET | RX_EVENT_ACK,
                pdTRUE,
                pdFALSE,
                pdMS_TO_TICKS(10));

            if ((bits & RX_EVENT_PACKET) != 0U) {
                drain_receive_queue();
            }

            if (((bits & RX_EVENT_ACK) != 0U) &&
                (s_last_acked_sequence == packet.sequence)) {
                ESP_LOGI(TAG, "Packet seq=%u acknowledged", packet.sequence);
                return ESP_OK;
            }
        }

        // Small backoff avoids repeated collisions with neighboring nodes.
        vTaskDelay(pdMS_TO_TICKS(15U + (attempt * 20U)));
    }

    return ESP_ERR_TIMEOUT;
}

/**
 * @brief Root task that periodically broadcasts synchronization beacons.
 *
 * Args:
 *     context: Unused task argument.
 */
static void root_beacon_task(void *context)
{
    (void)context;
    uint16_t beacon_sequence = 0U;

    while (true) {
        mesh_packet_t beacon = {
            .version = MESH_PROTOCOL_VERSION,
            .type = MESH_PACKET_BEACON,
            .source_id = CONFIG_APP_NODE_ID,
            .destination_id = MESH_BROADCAST_NODE_ID,
            .sequence = ++beacon_sequence,
            .ttl = 1U,
            .uptime_ms = (uint32_t)(esp_timer_get_time() / 1000ULL),
        };

        ESP_ERROR_CHECK_WITHOUT_ABORT(send_packet(BROADCAST_MAC, &beacon));
        vTaskDelay(pdMS_TO_TICKS(CONFIG_APP_BEACON_INTERVAL_MS));
    }
}

/**
 * @brief Always-awake receive task used by root and router nodes.
 *
 * Args:
 *     context: Unused task argument.
 */
static void receiver_task(void *context)
{
    (void)context;

    while (true) {
        EventBits_t bits = xEventGroupWaitBits(
            s_events,
            RX_EVENT_PACKET,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY);

        if ((bits & RX_EVENT_PACKET) != 0U) {
            drain_receive_queue();
        }
    }
}

/**
 * @brief Runs the complete wake, synchronize, transmit, and sleep cycle.
 * 
 * @return * void 
 */
static void run_leaf_cycle(void)
{
    ESP_LOGI(TAG, "Waiting up to %d ms for a synchronization beacon",
             CONFIG_APP_BEACON_WAIT_MS);

    const TickType_t wait_end = xTaskGetTickCount() +
                                pdMS_TO_TICKS(CONFIG_APP_BEACON_WAIT_MS);
    while (xTaskGetTickCount() < wait_end) {
        EventBits_t bits = xEventGroupWaitBits(
            s_events,
            RX_EVENT_PACKET | RX_EVENT_BEACON,
            pdTRUE,
            pdFALSE,
            pdMS_TO_TICKS(20));

        if ((bits & RX_EVENT_PACKET) != 0U) {
            drain_receive_queue();
        }
        if ((bits & RX_EVENT_BEACON) != 0U) {
            ESP_LOGI(TAG, "Synchronization beacon received");
            break;
        }
    }

    esp_err_t err = transmit_sensor_packet_with_retry();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Sensor packet was not acknowledged: %s",
                 esp_err_to_name(err));
    }

    // Stop Wi-Fi before deep sleep to minimize shutdown current transients.
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_now_deinit());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_stop());
    ESP_ERROR_CHECK(power_manager_enter_deep_sleep(
        CONFIG_APP_WAKE_INTERVAL_SEC));
}

/**
 * @brief Application entry point.
 */
void app_main(void)
{
    ++s_boot_count;

    // Initialize NVS, FreeRTOS queues, events, Wi-Fi, and ESP-NOW.
    ESP_ERROR_CHECK(initialize_nvs());
    
    // Create a queue for received packets and an event group for RX events.
    s_rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(rx_item_t));
    
    // Create an event group to signal received packets, beacons, and ACKs.
    s_events = xEventGroupCreate();
    if ((s_rx_queue == NULL) || (s_events == NULL)) {
        ESP_LOGE(TAG, "Failed to allocate FreeRTOS synchronization objects");
        abort();
    }

    // Initialize Wi-Fi in station mode on the configured ESP-NOW channel.
    ESP_ERROR_CHECK(initialize_wifi());
    
    // Initialize ESP-NOW, register callbacks, and add required peers.
    ESP_ERROR_CHECK(initialize_espnow());

    // Log the node role, ID, MAC address, channel, boot count, and wake-up cause.
    uint8_t local_mac[6];
    ESP_ERROR_CHECK(esp_read_mac(local_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG,
             "Role=%s node_id=%d MAC=%02X:%02X:%02X:%02X:%02X:%02X "
             "channel=%d boot=%lu wake=%s",
             node_role_name(),
             CONFIG_APP_NODE_ID,
             local_mac[0], local_mac[1], local_mac[2],
             local_mac[3], local_mac[4], local_mac[5],
             CONFIG_APP_WIFI_CHANNEL,
             (unsigned long)s_boot_count,
             power_manager_wakeup_cause_string());

    // Start the appropriate tasks or run the leaf cycle based on the node role.    
#if CONFIG_APP_ROLE_ROOT
    xTaskCreate(root_beacon_task, "root_beacon", 4096, NULL, 5, NULL);
    xTaskCreate(receiver_task, "mesh_rx", 4096, NULL, 6, NULL);
#elif CONFIG_APP_ROLE_ROUTER
    xTaskCreate(receiver_task, "mesh_rx", 4096, NULL, 6, NULL);
#else
    run_leaf_cycle();
#endif
}