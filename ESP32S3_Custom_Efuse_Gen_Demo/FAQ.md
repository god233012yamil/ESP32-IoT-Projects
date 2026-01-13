# Frequently Asked Questions (FAQ)

## General Questions

### What are eFuses?

eFuses (electronic fuses) are one-time programmable (OTP) bits in the ESP32-S3 chip. Once a bit is programmed from 0 to 1, it cannot be changed back. They're used to store critical device information like MAC addresses, encryption keys, calibration data, and custom user data.

### Why would I use custom eFuse fields?

Custom eFuse fields are useful for:
- **Device identification**: Serial numbers, manufacturing dates
- **Hardware configuration**: Hardware revision, feature flags
- **Calibration data**: Factory calibration coefficients
- **Security**: Device-specific keys or identifiers
- **Manufacturing**: Production line information, QC status

### Can I change eFuse values after programming?

**No.** eFuses are one-time programmable. Bits can only transition from 0 to 1, never back to 0. This is why testing with virtual eFuses is crucial before deploying to production.

## Project-Specific Questions

### What ESP-IDF version do I need?

This project requires ESP-IDF v5.0 or later. It has been tested with:
- ESP-IDF v5.0
- ESP-IDF v5.1
- ESP-IDF v5.2 (recommended)

### Does this work on other ESP32 chips?

The project is designed for ESP32-S3, but the approach can be adapted for:
- ESP32 (original)
- ESP32-C3
- ESP32-C6
- ESP32-S2

You'll need to modify the CSV table to match the eFuse block layout of your target chip.

### How do I know if I'm using virtual or real eFuses?

Check your configuration:
```bash
idf.py menuconfig
# Navigate to: Component config → eFuse Bit Manager
# Look for "Virtual eFuses" option
```

Or check `sdkconfig`:
```bash
grep CONFIG_EFUSE_VIRTUAL sdkconfig
```

If `CONFIG_EFUSE_VIRTUAL=y`, you're using virtual eFuses (safe mode).

## Build and Development

### The build fails with "efuse_table_gen.py not found"

**Solution**: Make sure your ESP-IDF environment is properly set up:
```bash
. $HOME/esp/esp-idf/export.sh  # Linux/macOS
# or
%IDF_TOOLS_PATH%\export.bat    # Windows
```

### Generated files are missing after build

**Solution**: Check that:
1. `main/esp_efuse_custom_table.csv` exists
2. CMake configuration is correct
3. Build directory is clean: `idf.py fullclean && idf.py build`

### Can I modify the CSV table after building?

Yes! After modifying `main/esp_efuse_custom_table.csv`:
```bash
idf.py build
```

The build system will automatically regenerate the C/H files.

### Why do I need batch mode for writing?

ESP32-S3 USER_DATA blocks (like EFUSE_BLK3) use Reed-Solomon error correction. Batch mode ensures:
1. All bits are staged together
2. Reed-Solomon encoding is calculated correctly
3. The entire block is programmed atomically

Without batch mode, the encoding will be incorrect.

## Runtime Questions

### How do I read eFuse values?

```c
uint32_t value;
esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_YOUR_FIELD, &value, 32);
```

### How do I write eFuse values?

```c
uint32_t value = 0x12345678;

// Begin batch
esp_efuse_batch_write_begin();

// Write field
esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_YOUR_FIELD, &value, 32);

// Commit (actually programs the bits)
esp_efuse_batch_write_commit();
```

### What happens if programming fails?

If programming fails:
1. The batch write returns an error code
2. Use `esp_efuse_batch_write_cancel()` to clean up
3. Check logs for the specific error
4. In virtual mode, nothing permanent happens
5. In real mode, some bits may have been programmed

### Can I program eFuses multiple times?

You can only set bits that are currently 0. Once a bit is 1, it stays 1 forever. So:
- ✅ Programming `0x00` → `0x0F` works
- ❌ Programming `0x0F` → `0x00` fails (bits can't go back to 0)
- ⚠️ Programming `0x0F` → `0xFF` works (only sets additional bits)

## Error Messages

### `ESP_ERR_EFUSE_CNT_IS_FULL`

**Meaning**: The eFuse bits you're trying to program are already set.

**Solution**: 
- Check if device is already provisioned
- In virtual mode: Delete `efuse_em.bin` or use `idf.py erase-flash`
- In real mode: Cannot fix - bits are permanently set

### `ESP_ERR_INVALID_STATE`

**Meaning**: eFuse operation called in wrong state (e.g., write without batch_begin).

**Solution**: 
- Always use `esp_efuse_batch_write_begin()` before writes
- Use `esp_efuse_batch_write_commit()` after writes

### CRC mismatch after reading

**Possible causes**:
1. Data was corrupted during programming
2. Byte order mismatch (endianness)
3. Payload construction doesn't match CRC calculation
4. Different CRC algorithm used for write vs read

**Solution**:
- Verify payload construction code
- Check byte order for multi-byte fields
- Ensure same CRC algorithm for both operations

## Safety and Best Practices

### What's the safest way to test?

1. **Always start with virtual eFuses** (`CONFIG_EFUSE_VIRTUAL=y`)
2. **Test all scenarios** in virtual mode
3. **Validate on a few dev boards** with real eFuses
4. **Document everything** before production
5. **Have a rollback plan** for manufacturing errors

### How do I protect against accidental programming?

1. Keep `CONFIG_EFUSE_VIRTUAL=y` in development
2. Disable `CONFIG_DEMO_PROGRAM_EFUSE` by default
3. Add confirmation prompts in production code
4. Implement proper authentication for programming
5. Use hardware write-protect mechanisms when available

### What should I avoid?

❌ **Don't**:
- Program eFuses without testing in virtual mode first
- Assume unused bits are 0 (they might already be 1)
- Program without CRC/checksum validation
- Use eFuses for frequently-changing data
- Forget that programming is permanent

✅ **Do**:
- Always test with virtual eFuses first
- Read back and verify after programming
- Implement proper error handling
- Use batch mode for user blocks
- Document your eFuse layout
- Plan for field expansion

## Production Use

### How do I adapt this for production?

1. **Modify the CSV table** for your specific fields
2. **Implement secure provisioning**:
   - Authentication before programming
   - Encrypted communication
   - Audit logging
3. **Add manufacturing database integration**
4. **Implement proper error handling and reporting**
5. **Create verification procedures**

### Can I use this in a commercial product?

Yes! This project is MIT licensed. You can:
- Use it in commercial products
- Modify it for your needs
- Redistribute it
- Must include the MIT license notice

### How do I handle manufacturing at scale?

1. **Create a provisioning system** that:
   - Generates unique serial numbers
   - Tracks which devices are programmed
   - Logs all programming operations
   - Handles errors gracefully

2. **Implement verification**:
   - Read back all values
   - Verify CRC/checksums
   - Test device functionality
   - Store results in database

3. **Have contingency plans**:
   - What if programming fails?
   - How to track partially-programmed devices?
   - RMA process for defective units

## Advanced Topics

### Can I use other eFuse blocks?

Yes, but be careful:
- **EFUSE_BLK0**: System reserved, don't use
- **EFUSE_BLK1**: Flash encryption key
- **EFUSE_BLK2**: Secure boot key
- **EFUSE_BLK3**: USER_DATA (this project uses this)
- **EFUSE_BLK4-10**: Additional key blocks

Always check ESP32-S3 documentation for block purposes.

### How do I implement custom CRC algorithms?

Modify the `crc16_ccitt_false()` function in `main.c`:

```c
// Example: CRC-32
static uint32_t crc32_custom(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    // Your CRC-32 implementation
    return crc;
}
```

Then update the CSV table to allocate 32 bits for CRC storage.

### Can I encrypt the eFuse data?

Yes, but:
1. Store encryption keys securely (possibly in other eFuse blocks)
2. Remember that eFuses are permanent - encrypted data can't be changed
3. Consider using ESP32's built-in encryption features

### How do I handle field versioning?

Include a version field in your CSV:

```csv
USER_DATA.VERSION,EFUSE_BLK3,0,8,Data format version
```

Then your code can handle different versions:

```c
uint8_t version;
esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_VERSION, &version, 8);

switch(version) {
    case 1: /* Handle v1 format */ break;
    case 2: /* Handle v2 format */ break;
    default: /* Unknown version */ break;
}
```

## Getting Help

### Where can I get more help?

1. **Read the documentation**:
   - Main README.md
   - ESP-IDF eFuse documentation
   - ESP32-S3 Technical Reference Manual

2. **Check existing issues** on GitHub

3. **Open a new issue** with:
   - Clear description
   - ESP-IDF version
   - Hardware details
   - Steps to reproduce
   - Logs/error messages

4. **Community resources**:
   - ESP32 Forum
   - ESP-IDF GitHub Discussions
   - Stack Overflow (tag: esp32)

### How do I report a bug?

1. Check if it's already reported
2. Gather information:
   - ESP-IDF version: `idf.py --version`
   - Hardware board
   - Configuration: `sdkconfig`
   - Full logs with debug enabled
3. Create minimal reproduction case
4. Open GitHub issue with all details

---

**Still have questions?** Open a GitHub issue or discussion!
