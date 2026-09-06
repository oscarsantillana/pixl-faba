# Validation and evidence

The published firmware package is the existing working release, not a new firmware build.

| Evidence | Result |
| --- | --- |
| faba10 host regression suite | Passed; original output in `evidence/faba10/tests.log` |
| KEYPAD firmware build | Passed; binary, HEX, ELF, map and build log retained |
| Package metadata and P-256 signature | Passed; extracted signature inputs and validation JSON retained |
| Historical DFU | Completed, 369540 bytes, CRC `8D9F8137`, 51.37 seconds |
| User hardware confirmation | Playback and Preferidos work; favourites survive restart |
| Historical final faba10 BLE readback | Timed out; no faba10 post-install running-version or saved-slot comparison claimed |
| Last successful historical BLE version verification | faba9 |

The pre-faba10 backup held configuration and three known slots. Both favourite banks were absent then. The owner's favourites created afterward were not included in that backup. Personal backups and device identifiers are not published here.

The host storage tests cover every partial-write length 0–527, corrupt-newest fallback, generation wrap, capacity, add/remove and filesystem failures. Queue tests cover capacity, wraparound, coalescing and no allocations during posting. Menu/player tests exercise repeated lifecycle and ownership paths.

These results do not establish full-story duration, playback of every catalogue entry, physical interrupt scheduling, or power interruption during a save. Other hardware variants and Faba+ were not tested. The newly portable Mac wrappers must still be checked against each installer's own device; compiling them is not a hardware transfer test.

The publication checks rerun host tests from this standalone source snapshot, compile the Mac tools, validate the unchanged release package, verify the signature and check the shipped catalogue hashes. Publication logs are under `evidence/publication/`. No device connection or flash is needed for these checks.

Archived build/test logs have their original workstation root replaced with `<original-workspace>`. The firmware binary remains byte-for-byte the release artifact. The ELF has two equal-length workstation-path replacements in non-loadable metadata. Its loadable segments and allocated sections are unchanged, so addresses and firmware contents remain matched to the release. The map retains the same address/symbol information. [Source provenance](../evidence/source-provenance.json) identifies pinned upstream bases and the archived patches.
