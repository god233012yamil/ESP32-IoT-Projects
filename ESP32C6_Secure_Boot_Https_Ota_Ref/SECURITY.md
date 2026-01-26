# Security Policy

## Supported Versions

This is a reference implementation. Security updates will be provided for the latest version on the main branch.

| Version | Supported          |
| ------- | ------------------ |
| Latest  | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability in this project, please report it responsibly:

### Do NOT
- Open a public GitHub issue for security vulnerabilities
- Disclose the vulnerability publicly before it has been addressed

### Please DO
1. Email security concerns to the project maintainers
2. Provide detailed information about the vulnerability:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

### What to Expect
- Acknowledgment of your report within 48 hours
- Regular updates on the progress of addressing the vulnerability
- Credit for responsible disclosure (if desired)

## Security Best Practices

### Production Deployment
- **NEVER** commit private signing keys to version control
- Store signing keys in Hardware Security Modules (HSM) or secure offline storage
- Implement proper key rotation policies
- Use certificate pinning with a backup certificate rotation strategy
- Enable anti-rollback protection in production
- Regularly audit and update dependencies

### Certificate Management
- Pin certificates with proper rotation plan
- Monitor certificate expiration dates
- Have backup certificates ready before rotation
- Test certificate updates in staging environment

### OTA Updates
- Always use HTTPS for firmware distribution
- Implement rate limiting on update servers
- Monitor for unusual update patterns
- Log and alert on failed update attempts
- Implement gradual rollout strategies

### Device Security
- Use Secure Boot v2 in production
- Enable Flash Encryption in Release mode
- Implement device attestation where possible
- Secure debug interfaces
- Disable unnecessary services and features

## Known Security Considerations

### Development Mode
- Development builds have relaxed security for testing
- Flash encryption is disabled by default in development
- Always use production configuration for deployed devices

### Battery Check Stub
- The included battery check is a demonstration stub
- Implement actual ADC-based battery monitoring before production
- Consider power failure scenarios during OTA

### Cloud Trigger Mechanism
- The included trigger URL check is minimal
- Implement proper device authentication for production
- Use device-specific tokens or certificates
- Implement request signing/verification

## Secure Boot Implementation

This project uses ESP32-C6 Secure Boot v2:
- RSA-PSS signatures for bootloader and application
- Hardware root of trust via eFuse storage
- Anti-rollback protection via secure version

### Key Management
- Generate keys with sufficient entropy
- Use key sizes as recommended by ESP-IDF documentation
- Never reuse keys across different product lines
- Implement proper key backup and recovery procedures

## Flash Encryption

Production devices should use Flash Encryption Release mode:
- One-time programmable configuration
- All flash contents encrypted at rest
- Encryption keys stored in hardware eFuses
- Cannot be disabled once enabled

## Additional Resources

- [ESP32-C6 Security Features](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/security/index.html)
- [Secure Boot Best Practices](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/security/secure-boot-v2.html)
- [Flash Encryption Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/security/flash-encryption.html)

---

**Remember**: This is a reference implementation. Conduct thorough security audits and penetration testing before production deployment.
