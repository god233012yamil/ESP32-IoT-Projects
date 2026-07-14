#include "ble_long_range.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_check.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define DEVICE_NAME                 "ESP32S3-LR-BLE"
#define PROFILE_APP_ID              0
#define SERVICE_INSTANCE_ID         0
#define EXT_ADV_INSTANCE            0
#define TELEMETRY_MAX_LENGTH        64
#define CONTROL_MAX_LENGTH          20
#define RSSI_FILTER_ALPHA_NUM       1
#define RSSI_FILTER_ALPHA_DEN       8
#define RSSI_STRONG_DBM             (-67)
#define RSSI_MODERATE_DBM           (-82)
#define RSSI_WEAK_DBM               (-92)
#define BLE_GAP_USE_EXPLICIT_PHY_MASKS 0

/* Custom 128-bit UUID base: 7a2eXXXX-5d9b-4d6f-a621-bd2c5b7a9011. */
static const uint8_t SERVICE_UUID[16] = {
    0x11, 0x90, 0x7a, 0x5b, 0x2c, 0xbd, 0x21, 0xa6,
    0x6f, 0x4d, 0x9b, 0x5d, 0x00, 0x10, 0x2e, 0x7a
};
static const uint8_t TELEMETRY_UUID[16] = {
    0x11, 0x90, 0x7a, 0x5b, 0x2c, 0xbd, 0x21, 0xa6,
    0x6f, 0x4d, 0x9b, 0x5d, 0x01, 0x10, 0x2e, 0x7a
};
static const uint8_t CONTROL_UUID[16] = {
    0x11, 0x90, 0x7a, 0x5b, 0x2c, 0xbd, 0x21, 0xa6,
    0x6f, 0x4d, 0x9b, 0x5d, 0x02, 0x10, 0x2e, 0x7a
};

static const char *TAG = "ble_long_range";
static esp_gatt_if_t s_gatts_if = ESP_GATT_IF_NONE;
static uint16_t s_connection_id;
static uint16_t s_connection_handle;
static esp_bd_addr_t s_peer_address;
static bool s_connected;
static bool s_notifications_enabled;
static int16_t s_filtered_rssi = -127;
static uint32_t s_rssi_samples;
static uint8_t s_current_phy = ESP_BLE_GAP_PHY_1M;
static esp_power_level_t s_current_power = ESP_PWR_LVL_P9;
static uint8_t s_telemetry_value[TELEMETRY_MAX_LENGTH] = "waiting-for-connection";
static uint8_t s_control_value[CONTROL_MAX_LENGTH] = "AUTO";

static const uint16_t PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t CHARACTER_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t CLIENT_CONFIG_UUID = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t CHAR_PROP_READ_NOTIFY = ESP_GATT_CHAR_PROP_BIT_READ |
                                              ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t CHAR_PROP_READ_WRITE = ESP_GATT_CHAR_PROP_BIT_READ |
                                             ESP_GATT_CHAR_PROP_BIT_WRITE;
static uint8_t CCCD_DEFAULT[2] = {0x00, 0x00};

/* Attribute table indexes. */
enum {
    IDX_SERVICE,
    IDX_TELEMETRY_DECL,
    IDX_TELEMETRY_VALUE,
    IDX_TELEMETRY_CCCD,
    IDX_CONTROL_DECL,
    IDX_CONTROL_VALUE,
    IDX_COUNT
};

static uint16_t s_handles[IDX_COUNT];

/**
 * @brief GATT database for the BLE long-range service. 
 * 
 */
static const esp_gatts_attr_db_t GATT_DATABASE[IDX_COUNT] = {
    [IDX_SERVICE] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&PRIMARY_SERVICE_UUID,
          ESP_GATT_PERM_READ, sizeof(SERVICE_UUID), sizeof(SERVICE_UUID),
          (uint8_t *)SERVICE_UUID}},

    [IDX_TELEMETRY_DECL] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&CHARACTER_DECLARATION_UUID,
          ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t),
          (uint8_t *)&CHAR_PROP_READ_NOTIFY}},

    [IDX_TELEMETRY_VALUE] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_128, (uint8_t *)TELEMETRY_UUID,
          ESP_GATT_PERM_READ, TELEMETRY_MAX_LENGTH,
          sizeof("waiting-for-connection") - 1, s_telemetry_value}},

    [IDX_TELEMETRY_CCCD] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&CLIENT_CONFIG_UUID,
          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
          sizeof(CCCD_DEFAULT), sizeof(CCCD_DEFAULT), CCCD_DEFAULT}},

    [IDX_CONTROL_DECL] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_16, (uint8_t *)&CHARACTER_DECLARATION_UUID,
          ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t),
          (uint8_t *)&CHAR_PROP_READ_WRITE}},

    [IDX_CONTROL_VALUE] =
        {{ESP_GATT_AUTO_RSP},
         {ESP_UUID_LEN_128, (uint8_t *)CONTROL_UUID,
          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
          CONTROL_MAX_LENGTH, sizeof("AUTO") - 1, s_control_value}},
};

/* Connectable extended advertising on LE Coded PHY. */
static const esp_ble_gap_ext_adv_params_t EXT_ADV_PARAMS = {
    .type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE,
    .interval_min = 0x00A0,  /* 100 ms in 0.625 ms units. */
    .interval_max = 0x00A0,
    .channel_map = ADV_CHNL_ALL,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    .tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE,
    .primary_phy = ESP_BLE_GAP_PHY_CODED,
    .max_skip = 0,
    .secondary_phy = ESP_BLE_GAP_PHY_CODED,
    .sid = 0,
    .scan_req_notif = false,
};

/* AD structure: flags, complete name, and complete 128-bit service UUID. */
static const uint8_t EXT_ADV_DATA[] = {
    0x02, ESP_BLE_AD_TYPE_FLAG, ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    0x0F, ESP_BLE_AD_TYPE_NAME_CMPL,
    'E', 'S', 'P', '3', '2', 'S', '3', '-', 'L', 'R', '-', 'B', 'L', 'E',
    0x11, ESP_BLE_AD_TYPE_128SRV_CMPL,
    0x11, 0x90, 0x7a, 0x5b, 0x2c, 0xbd, 0x21, 0xa6,
    0x6f, 0x4d, 0x9b, 0x5d, 0x00, 0x10, 0x2e, 0x7a
};

/**
 * Converts a PHY identifier to a readable label.
 *
 * Args:
 *     phy: ESP-IDF PHY identifier.
 *
 * Returns:
 *     A constant string describing the PHY.
 */
static const char *phy_to_string(uint8_t phy)
{
    switch (phy) {
        case ESP_BLE_GAP_PHY_1M:
            return "1M";
        case ESP_BLE_GAP_PHY_2M:
            return "2M";
        case ESP_BLE_GAP_PHY_CODED:
            return "CODED";
        default:
            return "UNKNOWN";
    }
}

/**
 * Updates the telemetry characteristic and optionally notifies the peer.
 *
 * Args:
 *     rssi: Most recent raw RSSI measurement in dBm.
 */
static void publish_telemetry(int8_t rssi)
{
    int length = snprintf((char *)s_telemetry_value,
                          sizeof(s_telemetry_value),
                          "rssi=%d,filtered=%d,phy=%s,pwr=%d,samples=%lu",
                          rssi,
                          s_filtered_rssi,
                          phy_to_string(s_current_phy),
                          (int)s_current_power,
                          (unsigned long)s_rssi_samples);

    if (length < 0) {
        ESP_LOGE(TAG, "Failed to format telemetry");
        return;
    }

    uint16_t value_length = (uint16_t)((length < TELEMETRY_MAX_LENGTH)
                                           ? length
                                           : TELEMETRY_MAX_LENGTH - 1);
    esp_err_t err = esp_ble_gatts_set_attr_value(s_handles[IDX_TELEMETRY_VALUE],
                                                  value_length,
                                                  s_telemetry_value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update telemetry: %s", esp_err_to_name(err));
        return;
    }

    if (s_connected && s_notifications_enabled) {
        err = esp_ble_gatts_send_indicate(s_gatts_if,
                                          s_connection_id,
                                          s_handles[IDX_TELEMETRY_VALUE],
                                          value_length,
                                          s_telemetry_value,
                                          false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Telemetry notification failed: %s", esp_err_to_name(err));
        }
    }
}

/**
 * Sets connection transmit power using the documented enhanced API.
 *
 * Args:
 *     power: ESP-IDF transmit-power level.
 */
static void set_connection_power(esp_power_level_t power)
{
    if (!s_connected) {
        return;
    }

    esp_err_t err = esp_ble_tx_power_set_enhanced(ESP_BLE_ENHANCED_PWR_TYPE_CONN,
                                                   s_connection_handle,
                                                   power);
    if (err == ESP_OK) {
        s_current_power = power;
    } else {
        ESP_LOGW(TAG, "Connection TX power update failed: %s", esp_err_to_name(err));
    }
}

/**
 * Requests a connection PHY using documented GAP APIs.
 *
 * Args:
 *     phy_mask: Requested transmit and receive PHY mask.
 *     options: Preferred coding option for LE Coded PHY.
 */
static void request_connection_phy(esp_ble_gap_phy_mask_t phy_mask,
                                   esp_ble_gap_prefer_phy_options_t options)
{
    if (!s_connected) {
        return;
    }

    esp_err_t err = esp_ble_gap_set_preferred_phy(s_peer_address,
                                                   BLE_GAP_USE_EXPLICIT_PHY_MASKS,
                                                   phy_mask,
                                                   phy_mask,
                                                   options);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "PHY request failed: %s", esp_err_to_name(err));
    }
}

/**
 * Applies an adaptive PHY and transmit-power policy from filtered RSSI.
 *
 * The policy uses hysteresis implicitly through the low-pass RSSI filter and
 * only changes radio settings when the desired operating region changes.
 */
static void apply_link_policy(void)
{
    if (s_filtered_rssi >= RSSI_STRONG_DBM) {
        set_connection_power(ESP_PWR_LVL_P3);
        request_connection_phy(ESP_BLE_GAP_PHY_1M_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
    } else if (s_filtered_rssi >= RSSI_MODERATE_DBM) {
        set_connection_power(ESP_PWR_LVL_P9);
        request_connection_phy(ESP_BLE_GAP_PHY_1M_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
    } else if (s_filtered_rssi >= RSSI_WEAK_DBM) {
        set_connection_power(ESP_PWR_LVL_P15);
        request_connection_phy(ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
    } else {
        set_connection_power(ESP_PWR_LVL_P20);
        request_connection_phy(ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING);
    }
}

/**
 * Processes a control characteristic command.
 *
 * Supported commands are AUTO, 1M, S2, S8, LOW, MEDIUM, HIGH, and MAX.
 *
 * Args:
 *     value: Pointer to the received command bytes.
 *     length: Number of command bytes.
 */
static void process_control_command(const uint8_t *value, uint16_t length)
{
    char command[CONTROL_MAX_LENGTH + 1] = {0};
    uint16_t copy_length = (length < CONTROL_MAX_LENGTH) ? length : CONTROL_MAX_LENGTH;
    memcpy(command, value, copy_length);

    /* Normalize lowercase ASCII commands to uppercase. */
    for (uint16_t i = 0; i < copy_length; ++i) {
        if (command[i] >= 'a' && command[i] <= 'z') {
            command[i] = (char)(command[i] - ('a' - 'A'));
        }
    }

    ESP_LOGI(TAG, "Control command: %s", command);

    if (strcmp(command, "AUTO") == 0) {
        apply_link_policy();
    } else if (strcmp(command, "1M") == 0) {
        request_connection_phy(ESP_BLE_GAP_PHY_1M_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
    } else if (strcmp(command, "S2") == 0) {
        request_connection_phy(ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S2_CODING);
    } else if (strcmp(command, "S8") == 0) {
        request_connection_phy(ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                               ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING);
    } else if (strcmp(command, "LOW") == 0) {
        set_connection_power(ESP_PWR_LVL_P3);
    } else if (strcmp(command, "MEDIUM") == 0) {
        set_connection_power(ESP_PWR_LVL_P9);
    } else if (strcmp(command, "HIGH") == 0) {
        set_connection_power(ESP_PWR_LVL_P15);
    } else if (strcmp(command, "MAX") == 0) {
        set_connection_power(ESP_PWR_LVL_P20);
    } else {
        ESP_LOGW(TAG, "Unknown command");
    }
}

/**
 * Starts the configured extended advertising set.
 */
static void start_extended_advertising(void)
{
    const esp_ble_gap_ext_adv_t advertising_set = {
        .instance = EXT_ADV_INSTANCE,
        .duration = 0,
        .max_events = 0,
    };

    esp_err_t err = esp_ble_gap_ext_adv_start(1, &advertising_set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start extended advertising: %s", esp_err_to_name(err));
    }
}

/**
 * Periodically requests RSSI measurements while a peer is connected.
 *
 * Args:
 *     context: Unused FreeRTOS task context pointer.
 */
static void rssi_monitor_task(void *context)
{
    (void)context;

    while (true) {
        if (s_connected) {
            esp_err_t err = esp_ble_gap_read_rssi(s_peer_address);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "RSSI request failed: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Handles Bluetooth GAP events.
 *
 * Args:
 *     event: GAP event identifier.
 *     param: Event-specific parameter union.
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
            if (param->ext_adv_set_params.status == ESP_BT_STATUS_SUCCESS) {
                ESP_ERROR_CHECK(esp_ble_gap_config_ext_adv_data_raw(
                    EXT_ADV_INSTANCE, sizeof(EXT_ADV_DATA), EXT_ADV_DATA));
            } else {
                ESP_LOGE(TAG, "Extended advertising parameter setup failed");
            }
            break;

        case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
            if (param->ext_adv_data_set.status == ESP_BT_STATUS_SUCCESS) {
                start_extended_advertising();
            } else {
                ESP_LOGE(TAG, "Extended advertising data setup failed");
            }
            break;

        case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "Extended Coded PHY advertising status=%d",
                     param->ext_adv_start.status);
            break;

        case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
            if (param->phy_update.status == ESP_BT_STATUS_SUCCESS) {
                s_current_phy = param->phy_update.tx_phy;
                ESP_LOGI(TAG, "PHY updated: TX=%s RX=%s",
                         phy_to_string(param->phy_update.tx_phy),
                         phy_to_string(param->phy_update.rx_phy));
            }
            break;

        case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
            if (param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                int8_t raw_rssi = param->read_rssi_cmpl.rssi;
                if (s_rssi_samples == 0) {
                    s_filtered_rssi = raw_rssi;
                } else {
                    s_filtered_rssi = (int16_t)(
                        (RSSI_FILTER_ALPHA_NUM * raw_rssi +
                         (RSSI_FILTER_ALPHA_DEN - RSSI_FILTER_ALPHA_NUM) * s_filtered_rssi) /
                        RSSI_FILTER_ALPHA_DEN);
                }
                ++s_rssi_samples;
                publish_telemetry(raw_rssi);
                apply_link_policy();

            }
            break;

        default:
            break;
    }
}

/**
 * Handles Bluetooth GATT server events.
 *
 * Args:
 *     event: GATT server event identifier.
 *     gatts_if: Interface assigned to this GATT application.
 *     param: Event-specific parameter union.
 */
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "GATT registration failed: %d", param->reg.status);
                return;
            }
            s_gatts_if = gatts_if;
            ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
            ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(GATT_DATABASE,
                                                           gatts_if,
                                                           IDX_COUNT,
                                                           SERVICE_INSTANCE_ID));
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status != ESP_GATT_OK ||
                param->add_attr_tab.num_handle != IDX_COUNT) {
                ESP_LOGE(TAG, "Attribute table creation failed");
                return;
            }
            memcpy(s_handles, param->add_attr_tab.handles, sizeof(s_handles));
            ESP_ERROR_CHECK(esp_ble_gatts_start_service(s_handles[IDX_SERVICE]));
            ESP_ERROR_CHECK(esp_ble_gap_ext_adv_set_params(EXT_ADV_INSTANCE,
                                                            &EXT_ADV_PARAMS));
            break;

        case ESP_GATTS_CONNECT_EVT:
            s_connected = true;
            s_connection_id = param->connect.conn_id;
            s_connection_handle = param->connect.conn_handle;
            memcpy(s_peer_address, param->connect.remote_bda, sizeof(esp_bd_addr_t));
            s_filtered_rssi = -127;
            s_rssi_samples = 0;
            ESP_LOGI(TAG, "Peer connected, conn_id=%u handle=%u",
                     s_connection_id, s_connection_handle);

            /* Maximum documented ESP32-S3 connection power for range testing. */
            set_connection_power(ESP_PWR_LVL_P20);
            request_connection_phy(ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                                   ESP_BLE_GAP_PHY_OPTIONS_PREF_S8_CODING);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            ESP_LOGI(TAG, "Peer disconnected, reason=0x%02x", param->disconnect.reason);
            s_connected = false;
            s_notifications_enabled = false;
            s_current_phy = ESP_BLE_GAP_PHY_1M;
            start_extended_advertising();
            break;

        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == s_handles[IDX_TELEMETRY_CCCD] &&
                param->write.len == 2) {
                uint16_t cccd = (uint16_t)param->write.value[0] |
                                ((uint16_t)param->write.value[1] << 8);
                s_notifications_enabled = (cccd == 0x0001);
                ESP_LOGI(TAG, "Notifications %s",
                         s_notifications_enabled ? "enabled" : "disabled");
            } else if (param->write.handle == s_handles[IDX_CONTROL_VALUE]) {
                process_control_command(param->write.value, param->write.len);
            }
            break;

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "Negotiated ATT MTU=%u", param->mtu.mtu);
            break;

        default:
            break;
    }
}

/**
 * Initializes NVS while recovering from partition-version mismatches.
 *
 * Returns:
 *     ESP_OK: NVS is ready.
 *     Other: An unrecoverable NVS error.
 */
static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        err = nvs_flash_init();
    }
    return err;
}

/**
 * Initializes the Bluetooth controller and Bluedroid host stack.
 *
 * Returns:
 *     ESP_OK: Bluetooth is ready.
 *     Other: An ESP-IDF Bluetooth initialization error.
 */
static esp_err_t initialize_bluetooth_stack(void)
{
    esp_bt_controller_config_t controller_config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_RETURN_ON_ERROR(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT),
                        TAG, "Classic Bluetooth memory release failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&controller_config),
                        TAG, "Bluetooth controller init failed");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_BLE),
                        TAG, "Bluetooth controller enable failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "Bluedroid init failed");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "Bluedroid enable failed");
    return ESP_OK;
}

/**
 * Initializes the BLE long-range GATT server.
 *
 * Returns:
 *     ESP_OK: Initialization completed successfully.
 *     Other: An ESP-IDF error code identifying the failed operation.
 */
esp_err_t ble_long_range_init(void)
{
    ESP_RETURN_ON_ERROR(initialize_nvs(), TAG, "NVS initialization failed");
    ESP_RETURN_ON_ERROR(initialize_bluetooth_stack(),
                        TAG, "Bluetooth stack initialization failed");

    ESP_RETURN_ON_ERROR(esp_ble_gatts_register_callback(gatts_event_handler),
                        TAG, "GATT callback registration failed");
    ESP_RETURN_ON_ERROR(esp_ble_gap_register_callback(gap_event_handler),
                        TAG, "GAP callback registration failed");
    ESP_RETURN_ON_ERROR(esp_ble_gatts_app_register(PROFILE_APP_ID),
                        TAG, "GATT application registration failed");
    ESP_RETURN_ON_ERROR(esp_ble_gatt_set_local_mtu(247),
                        TAG, "Local MTU setup failed");

    /* Prefer Coded PHY for future connections; the peer may negotiate another PHY. */
    ESP_RETURN_ON_ERROR(esp_ble_gap_set_preferred_default_phy(
                            ESP_BLE_GAP_PHY_CODED_PREF_MASK,
                            ESP_BLE_GAP_PHY_CODED_PREF_MASK),
                        TAG, "Default PHY setup failed");

    /* Set default and advertising power through documented controller APIs. */
    ESP_RETURN_ON_ERROR(esp_ble_tx_power_set_enhanced(
                            ESP_BLE_ENHANCED_PWR_TYPE_DEFAULT,
                            0,
                            ESP_PWR_LVL_P20),
                        TAG, "Default TX power setup failed");
    ESP_RETURN_ON_ERROR(esp_ble_tx_power_set_enhanced(
                            ESP_BLE_ENHANCED_PWR_TYPE_ADV,
                            EXT_ADV_INSTANCE,
                            ESP_PWR_LVL_P20),
                        TAG, "Advertising TX power setup failed");

    BaseType_t task_created = xTaskCreate(rssi_monitor_task,
                                           "ble_rssi_monitor",
                                           3072,
                                           NULL,
                                           5,
                                           NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RSSI monitor task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
