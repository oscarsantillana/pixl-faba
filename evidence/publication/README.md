# Publication checks

The standalone source snapshot passes the full host regression suite. The archived application patch's 38 files and Chameleon patch's one file were reconstructed against their pinned upstream commits and compared byte-for-byte with the published source.

All firmware and catalogue checksums, Nordic package metadata and extracted signature evidence match. CryptoKit verifies the P-256 signature against the upstream bootloader public key. Primary installation/build/technical guide links resolve. All six application BLE utilities reject a missing UUID before Bluetooth access.

No Bluetooth device connection or firmware transfer was performed to publish this repository. The tools' new configurable UUID handling and strict catalogue missing-file checks have compile/host validation, not a new hardware validation.
