#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"

#define AES_KEY_SIZE_BYTES       32U
#define GCM_IV_SIZE_BYTES        12U
#define GCM_TAG_SIZE_BYTES       16U
#define SHA256_DIGEST_SIZE_BYTES 32U
#define BENCHMARK_BUFFER_SIZE    1024U
#define BENCHMARK_ITERATIONS     1000U

static const char *TAG = "HW_CRYPTO";

/**
 * @brief Prints a byte buffer as uppercase hexadecimal text.
 *
 * @param label Label printed before the hexadecimal bytes.
 * @param data Pointer to the byte buffer.
 * @param length Number of bytes to print.
 */
static void print_hex(const char *label, const uint8_t *data, size_t length)
{
    printf("%s", label);

    for (size_t index = 0; index < length; ++index) {
        printf("%02X", data[index]);
    }

    printf("\n");
}

/**
 * @brief Compares two byte buffers without early exit.
 *
 * This helper is suitable for comparing public test results. Protocol code
 * should use the authentication functions supplied by the crypto library.
 *
 * @param left Pointer to the first buffer.
 * @param right Pointer to the second buffer.
 * @param length Number of bytes to compare.
 *
 * @return true when all bytes are equal; otherwise false.
 */
static bool buffers_equal(const uint8_t *left, const uint8_t *right, size_t length)
{
    uint8_t difference = 0U;

    for (size_t index = 0; index < length; ++index) {
        difference |= (uint8_t)(left[index] ^ right[index]);
    }

    return difference == 0U;
}

/**
 * @brief Fills a buffer with bytes from the ESP32-S3 hardware RNG.
 *
 * @param buffer Destination buffer.
 * @param length Number of random bytes to generate.
 */
static void fill_random_bytes(uint8_t *buffer, size_t length)
{
    size_t offset = 0U;

    while (offset < length) {
        uint32_t random_word = esp_random();
        size_t remaining = length - offset;
        size_t copy_size = remaining < sizeof(random_word)
                         ? remaining
                         : sizeof(random_word);

        memcpy(&buffer[offset], &random_word, copy_size);
        offset += copy_size;
    }
}

/**
 * @brief Increments the 32-bit counter stored in the last four IV bytes.
 *
 * The project uses this helper only for its local benchmark. A production
 * protocol must persist or otherwise guarantee nonce uniqueness across resets.
 *
 * @param iv Pointer to a 12-byte GCM initialization vector.
 */
static void increment_iv_counter(uint8_t iv[GCM_IV_SIZE_BYTES])
{
    for (int index = (int)GCM_IV_SIZE_BYTES - 1; index >= 8; --index) {
        iv[index]++;

        if (iv[index] != 0U) {
            break;
        }
    }
}

/**
 * @brief Encrypts and authenticates data with AES-256-GCM.
 *
 * ESP-IDF routes supported AES block operations through the ESP32-S3 hardware
 * accelerator when CONFIG_MBEDTLS_HARDWARE_AES is enabled.
 *
 * @param key Pointer to a 32-byte AES key.
 * @param iv Pointer to the initialization vector.
 * @param iv_length Initialization-vector length in bytes.
 * @param aad Pointer to optional additional authenticated data, or NULL.
 * @param aad_length Additional authenticated-data length in bytes.
 * @param plaintext Pointer to plaintext input.
 * @param plaintext_length Plaintext length in bytes.
 * @param ciphertext Destination buffer for ciphertext.
 * @param tag Destination buffer for the 16-byte authentication tag.
 *
 * @return ESP_OK on success; otherwise ESP_FAIL.
 */
static esp_err_t aes_gcm_encrypt(
    const uint8_t key[AES_KEY_SIZE_BYTES],
    const uint8_t *iv,
    size_t iv_length,
    const uint8_t *aad,
    size_t aad_length,
    const uint8_t *plaintext,
    size_t plaintext_length,
    uint8_t *ciphertext,
    uint8_t tag[GCM_TAG_SIZE_BYTES])
{
    mbedtls_gcm_context context;
    int result;

    // Initialize the GCM context and set the AES key.
    mbedtls_gcm_init(&context);

    // Set the AES key for GCM mode. The key length is specified in bits.
    result = mbedtls_gcm_setkey(
        &context,
        MBEDTLS_CIPHER_ID_AES,
        key,
        AES_KEY_SIZE_BYTES * 8U);

    if (result != 0) {
        ESP_LOGE(TAG, "mbedtls_gcm_setkey failed: -0x%04X", -result);
        mbedtls_gcm_free(&context);
        return ESP_FAIL;
    }

    // Encrypt the plaintext and generate the authentication tag.
    result = mbedtls_gcm_crypt_and_tag(
        &context,
        MBEDTLS_GCM_ENCRYPT,
        plaintext_length,
        iv,
        iv_length,
        aad,
        aad_length,
        plaintext,
        ciphertext,
        GCM_TAG_SIZE_BYTES,
        tag);

    // Free the GCM context.
    mbedtls_gcm_free(&context);

    if (result != 0) {
        ESP_LOGE(TAG, "mbedtls_gcm_crypt_and_tag failed: -0x%04X", -result);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Authenticates and decrypts AES-256-GCM data.
 *
 * @param key Pointer to a 32-byte AES key.
 * @param iv Pointer to the initialization vector.
 * @param iv_length Initialization-vector length in bytes.
 * @param aad Pointer to optional additional authenticated data, or NULL.
 * @param aad_length Additional authenticated-data length in bytes.
 * @param ciphertext Pointer to ciphertext input.
 * @param ciphertext_length Ciphertext length in bytes.
 * @param tag Pointer to the 16-byte authentication tag.
 * @param plaintext Destination buffer for recovered plaintext.
 *
 * @return ESP_OK when authentication and decryption succeed.
 * @return ESP_ERR_INVALID_CRC when tag verification fails.
 * @return ESP_FAIL for other cryptographic errors.
 */
static esp_err_t aes_gcm_decrypt(
    const uint8_t key[AES_KEY_SIZE_BYTES],
    const uint8_t *iv,
    size_t iv_length,
    const uint8_t *aad,
    size_t aad_length,
    const uint8_t *ciphertext,
    size_t ciphertext_length,
    const uint8_t tag[GCM_TAG_SIZE_BYTES],
    uint8_t *plaintext)
{
    mbedtls_gcm_context context;
    int result;

    // Initialize the GCM context.
    mbedtls_gcm_init(&context);

    // Set the AES key for GCM mode. The key length is specified in bits.
    result = mbedtls_gcm_setkey(
        &context,
        MBEDTLS_CIPHER_ID_AES,
        key,
        AES_KEY_SIZE_BYTES * 8U);

    if (result != 0) {
        ESP_LOGE(TAG, "mbedtls_gcm_setkey failed: -0x%04X", -result);
        mbedtls_gcm_free(&context);
        return ESP_FAIL;
    }

    // Authenticate and decrypt the ciphertext.
    result = mbedtls_gcm_auth_decrypt(
        &context,
        ciphertext_length,
        iv,
        iv_length,
        aad,
        aad_length,
        tag,
        GCM_TAG_SIZE_BYTES,
        ciphertext,
        plaintext);

    // Free the GCM context.
    mbedtls_gcm_free(&context);

    if (result == MBEDTLS_ERR_GCM_AUTH_FAILED) {
        return ESP_ERR_INVALID_CRC;
    }

    if (result != 0) {
        ESP_LOGE(TAG, "mbedtls_gcm_auth_decrypt failed: -0x%04X", -result);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Calculates a SHA-256 digest using Mbed TLS.
 *
 * ESP-IDF routes supported SHA operations through the ESP32-S3 SHA peripheral
 * when CONFIG_MBEDTLS_HARDWARE_SHA is enabled.
 *
 * @param data Pointer to input data.
 * @param length Input length in bytes.
 * @param digest Destination buffer for the 32-byte digest.
 *
 * @return ESP_OK on success; otherwise ESP_FAIL.
 */
static esp_err_t calculate_sha256(
    const uint8_t *data,
    size_t length,
    uint8_t digest[SHA256_DIGEST_SIZE_BYTES])
{
    int result = mbedtls_sha256(data, length, digest, 0);

    if (result != 0) {
        ESP_LOGE(TAG, "mbedtls_sha256 failed: -0x%04X", -result);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Runs a known-answer AES-256-GCM validation test.
 *
 * The vector is from NIST GCM test material for an all-zero 256-bit key,
 * 96-bit IV, and one all-zero plaintext block.
 *
 * @return ESP_OK when ciphertext, tag, and decrypted plaintext are correct.
 */
static esp_err_t run_aes_gcm_known_answer_test(void)
{
    static const uint8_t key[AES_KEY_SIZE_BYTES] = {0};
    static const uint8_t iv[GCM_IV_SIZE_BYTES] = {0};
    static const uint8_t plaintext[16] = {0};
    static const uint8_t expected_ciphertext[16] = {
        0xCE, 0xA7, 0x40, 0x3D, 0x4D, 0x60, 0x6B, 0x6E,
        0x07, 0x4E, 0xC5, 0xD3, 0xBA, 0xF3, 0x9D, 0x18
    };
    static const uint8_t expected_tag[GCM_TAG_SIZE_BYTES] = {
        0xD0, 0xD1, 0xC8, 0xA7, 0x99, 0x99, 0x6B, 0xF0,
        0x26, 0x5B, 0x98, 0xB5, 0xD4, 0x8A, 0xB9, 0x19
    };

    uint8_t ciphertext[sizeof(plaintext)] = {0};
    uint8_t decrypted[sizeof(plaintext)] = {0};
    uint8_t tag[GCM_TAG_SIZE_BYTES] = {0};

    // Encrypt the known plaintext and generate the authentication tag.
    ESP_RETURN_ON_ERROR(
        aes_gcm_encrypt(
            key,
            iv,
            sizeof(iv),
            NULL,
            0U,
            plaintext,
            sizeof(plaintext),
            ciphertext,
            tag),
        TAG,
        "AES-GCM encryption failed");

    // Check if the generated ciphertext and tag match the expected values.
    if (!buffers_equal(ciphertext, expected_ciphertext, sizeof(ciphertext)) ||
        !buffers_equal(tag, expected_tag, sizeof(tag))) {
        ESP_LOGE(TAG, "AES-GCM known-answer test failed");
        print_hex("Actual ciphertext: ", ciphertext, sizeof(ciphertext));
        print_hex("Actual tag:        ", tag, sizeof(tag));
        return ESP_FAIL;
    }

    // Decrypt the ciphertext and verify that the recovered plaintext matches the original.
    ESP_RETURN_ON_ERROR(
        aes_gcm_decrypt(
            key,
            iv,
            sizeof(iv),
            NULL,
            0U,
            ciphertext,
            sizeof(ciphertext),
            tag,
            decrypted),
        TAG,
        "AES-GCM decryption failed");

    if (!buffers_equal(decrypted, plaintext, sizeof(plaintext))) {
        ESP_LOGE(TAG, "AES-GCM recovered plaintext does not match");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "AES-256-GCM known-answer test: PASS");
    return ESP_OK;
}

/**
 * @brief Runs a SHA-256 known-answer validation test.
 *
 * @return ESP_OK when the digest of "abc" matches the standard result.
 */
static esp_err_t run_sha256_known_answer_test(void)
{
    static const uint8_t message[] = "abc";
    static const uint8_t expected_digest[SHA256_DIGEST_SIZE_BYTES] = {
        0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA,
        0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
        0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C,
        0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD
    };

    uint8_t digest[SHA256_DIGEST_SIZE_BYTES] = {0};

    // Calculate the SHA-256 digest of the message.
    ESP_RETURN_ON_ERROR(
        calculate_sha256(message, sizeof(message) - 1U, digest),
        TAG,
        "SHA-256 operation failed");

    // Check if the calculated digest matches the expected value.
    if (!buffers_equal(digest, expected_digest, sizeof(digest))) {
        ESP_LOGE(TAG, "SHA-256 known-answer test failed");
        print_hex("Actual digest: ", digest, sizeof(digest));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "SHA-256 known-answer test: PASS");
    return ESP_OK;
}

/**
 * @brief Demonstrates encryption, authenticated decryption, and tamper detection.
 *
 * @return ESP_OK when all demonstration checks pass.
 */
static esp_err_t run_authenticated_encryption_demo(void)
{
    static const uint8_t plaintext[] =
        "Temperature=24.7,Humidity=48.2";
    static const uint8_t aad[] =
        "device=esp32s3-node-01;type=telemetry";

    uint8_t key[AES_KEY_SIZE_BYTES];
    uint8_t iv[GCM_IV_SIZE_BYTES];
    uint8_t ciphertext[sizeof(plaintext)] = {0};
    uint8_t decrypted[sizeof(plaintext)] = {0};
    uint8_t tag[GCM_TAG_SIZE_BYTES] = {0};
    uint8_t tampered_ciphertext[sizeof(ciphertext)] = {0};

    // Generate a random AES key and IV for the demonstration.
    fill_random_bytes(key, sizeof(key));
    fill_random_bytes(iv, sizeof(iv));

    // Encrypt the plaintext and generate the authentication tag.
    ESP_RETURN_ON_ERROR(
        aes_gcm_encrypt(
            key,
            iv,
            sizeof(iv),
            aad,
            sizeof(aad) - 1U,
            plaintext,
            sizeof(plaintext),
            ciphertext,
            tag),
        TAG,
        "Authenticated encryption failed");

    // Decrypt the ciphertext and verify that the recovered plaintext matches the original.
    ESP_RETURN_ON_ERROR(
        aes_gcm_decrypt(
            key,
            iv,
            sizeof(iv),
            aad,
            sizeof(aad) - 1U,
            ciphertext,
            sizeof(ciphertext),
            tag,
            decrypted),
        TAG,
        "Authenticated decryption failed");

    // Check if the decrypted plaintext matches the original plaintext.
    if (!buffers_equal(decrypted, plaintext, sizeof(plaintext))) {
        ESP_LOGE(TAG, "Round-trip plaintext mismatch");
        return ESP_FAIL;
    }

    print_hex("IV:         ", iv, sizeof(iv));
    print_hex("Ciphertext: ", ciphertext, sizeof(ciphertext));
    print_hex("GCM tag:    ", tag, sizeof(tag));
    ESP_LOGI(TAG, "Recovered plaintext: %s", (char *)decrypted);

    // Tamper with the ciphertext and verify that decryption fails.
    memcpy(tampered_ciphertext, ciphertext, sizeof(ciphertext));
    tampered_ciphertext[0] ^= 0x01U;
    memset(decrypted, 0, sizeof(decrypted));

    // Attempt to decrypt the tampered ciphertext. The operation should fail with ESP_ERR_INVALID_CRC.
    esp_err_t tamper_result = aes_gcm_decrypt(
        key,
        iv,
        sizeof(iv),
        aad,
        sizeof(aad) - 1U,
        tampered_ciphertext,
        sizeof(tampered_ciphertext),
        tag,
        decrypted);

    if (tamper_result != ESP_ERR_INVALID_CRC) {
        ESP_LOGE(TAG, "Tampered ciphertext was not rejected");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Tamper detection: PASS");

    // Remove transient secrets and plaintext from RAM after use.
    memset(key, 0, sizeof(key));
    memset(decrypted, 0, sizeof(decrypted));

    return ESP_OK;
}

/**
 * @brief Measures AES-256-GCM throughput for 1024-byte buffers.
 *
 * The test generates one random key and one random IV prefix, then increments a
 * 32-bit counter in the IV for every operation. It reports aggregate throughput
 * and average latency. This is an application benchmark, not a silicon rating.
 *
 * @return ESP_OK when all benchmark operations complete successfully.
 */
static esp_err_t benchmark_aes_gcm(void)
{
    uint8_t key[AES_KEY_SIZE_BYTES];
    uint8_t iv[GCM_IV_SIZE_BYTES];
    uint8_t input[BENCHMARK_BUFFER_SIZE];
    uint8_t output[BENCHMARK_BUFFER_SIZE];
    uint8_t tag[GCM_TAG_SIZE_BYTES];

    fill_random_bytes(key, sizeof(key));
    fill_random_bytes(iv, sizeof(iv));
    fill_random_bytes(input, sizeof(input));

    int64_t start_time_us = esp_timer_get_time();

    // Perform multiple AES-GCM operations, incrementing the IV counter each time.
    for (uint32_t iteration = 0U;
         iteration < BENCHMARK_ITERATIONS;
         ++iteration) {
        increment_iv_counter(iv);

        esp_err_t result = aes_gcm_encrypt(
            key,
            iv,
            sizeof(iv),
            NULL,
            0U,
            input,
            sizeof(input),
            output,
            tag);

        if (result != ESP_OK) {
            ESP_LOGE(TAG, "AES benchmark failed at iteration %" PRIu32, iteration);
            return result;
        }
    }

    int64_t elapsed_time_us = esp_timer_get_time() - start_time_us;
    uint64_t total_bytes =
        (uint64_t)BENCHMARK_ITERATIONS * BENCHMARK_BUFFER_SIZE;
    double elapsed_seconds = (double)elapsed_time_us / 1000000.0;
    double throughput_mib_s =
        ((double)total_bytes / (1024.0 * 1024.0)) / elapsed_seconds;
    double average_latency_us =
        (double)elapsed_time_us / (double)BENCHMARK_ITERATIONS;

    ESP_LOGI(
        TAG,
        "AES-GCM benchmark: %" PRIu64 " bytes in %" PRId64 " us",
        total_bytes,
        elapsed_time_us);
    ESP_LOGI(TAG, "Throughput: %.2f MiB/s", throughput_mib_s);
    ESP_LOGI(TAG, "Average latency: %.2f us per 1024-byte packet", average_latency_us);

    memset(key, 0, sizeof(key));
    memset(output, 0, sizeof(output));
    return ESP_OK;
}

/**
 * @brief Measures SHA-256 throughput for 1024-byte buffers.
 *
 * @return ESP_OK when all benchmark operations complete successfully.
 */
static esp_err_t benchmark_sha256(void)
{
    uint8_t input[BENCHMARK_BUFFER_SIZE];
    uint8_t digest[SHA256_DIGEST_SIZE_BYTES];

    fill_random_bytes(input, sizeof(input));

    int64_t start_time_us = esp_timer_get_time();

    // Perform multiple SHA-256 operations, modifying the input buffer each time.
    for (uint32_t iteration = 0U;
         iteration < BENCHMARK_ITERATIONS;
         ++iteration) {
        esp_err_t result = calculate_sha256(input, sizeof(input), digest);

        if (result != ESP_OK) {
            ESP_LOGE(TAG, "SHA benchmark failed at iteration %" PRIu32, iteration);
            return result;
        }

        // Change one byte so every iteration hashes different input data.
        input[iteration % sizeof(input)]++;
    }

    int64_t elapsed_time_us = esp_timer_get_time() - start_time_us;
    uint64_t total_bytes =
        (uint64_t)BENCHMARK_ITERATIONS * BENCHMARK_BUFFER_SIZE;
    double elapsed_seconds = (double)elapsed_time_us / 1000000.0;
    double throughput_mib_s =
        ((double)total_bytes / (1024.0 * 1024.0)) / elapsed_seconds;
    double average_latency_us =
        (double)elapsed_time_us / (double)BENCHMARK_ITERATIONS;

    ESP_LOGI(
        TAG,
        "SHA-256 benchmark: %" PRIu64 " bytes in %" PRId64 " us",
        total_bytes,
        elapsed_time_us);
    ESP_LOGI(TAG, "Throughput: %.2f MiB/s", throughput_mib_s);
    ESP_LOGI(TAG, "Average latency: %.2f us per 1024-byte buffer", average_latency_us);
    print_hex("Final SHA-256 digest: ", digest, sizeof(digest));

    return ESP_OK;
}

/**
 * @brief Prints basic chip and build configuration information.
 */
static void print_platform_information(void)
{
    // Print ESP32-S3 chip information.
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Chip cores: %u", chip_info.cores);
    ESP_LOGI(TAG, "Silicon revision: %u", chip_info.revision);

#if CONFIG_MBEDTLS_HARDWARE_AES
    ESP_LOGI(TAG, "Mbed TLS hardware AES: enabled");
#else
    ESP_LOGW(TAG, "Mbed TLS hardware AES: disabled");
#endif

#if CONFIG_MBEDTLS_HARDWARE_SHA
    ESP_LOGI(TAG, "Mbed TLS hardware SHA: enabled");
#else
    ESP_LOGW(TAG, "Mbed TLS hardware SHA: disabled");
#endif
}

/**
 * @brief Application entry point.
 *
 * Runs known-answer validation, an authenticated-encryption demonstration,
 * tamper detection, and AES/SHA performance benchmarks.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 hardware-accelerated cryptography demo");
    print_platform_information();

    // Run AES-GCM known-answer test.
    ESP_ERROR_CHECK(run_aes_gcm_known_answer_test());
    
    // Run SHA-256 known-answer test.
    ESP_ERROR_CHECK(run_sha256_known_answer_test());
    
    // Run authenticated-encryption demonstration and tamper detection.
    ESP_ERROR_CHECK(run_authenticated_encryption_demo());
    
    // Run AES-GCM benchmark first because it does not modify the input buffer.
    ESP_ERROR_CHECK(benchmark_aes_gcm());
    
    // Run SHA-256 benchmark last because it modifies the input buffer on every iteration.
    ESP_ERROR_CHECK(benchmark_sha256());

    ESP_LOGI(TAG, "All cryptographic tests completed successfully");
}
