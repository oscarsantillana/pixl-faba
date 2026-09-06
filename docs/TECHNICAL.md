# Technical notes and investigation

## Why stock emulation failed

A phone could read the emulated NTAG213 while the original Faba stayed silent. Captures showed Faba sending a valid `READ 07` after cascade-level-1 selection and before cascade-level-2 selection finished. The NFC state machine was still READY and rejected it.

The local Chameleon change accepts only that four-byte, valid-CRC READ of page 7 in READY, for NTAG213 with Faba compatibility active. Other pages, bad CRCs, other tag types and inactive compatibility remain rejected. The captured Faba replay improved from zero to three delivered reads; the phone control retained six. Close antenna placement also mattered on hardware.

The physical reader trace includes interleaved parity. Decode it before interpreting bytes or CRCs. Retained traces keep the first 32 and last 96 events. Peripheral event timing was about 62.56 microseconds from RXEND to TXSTART; this was not an external RF measurement.

## Playback ownership and UI memory

The catalogue player owns a private 497-byte NTAG213 profile and changes only the four story-ID digits. It disables sleep while active, stops NFC on exit, restores saved-slot handler ownership before persistence, then frees its buffer and restores power settings. Temporary catalogue playback must not overwrite a saved NFC slot.

The NDEF text is `02190530` + a four-digit story ID + `00`. NDEF metadata `en` does not determine the story's spoken language. The working profile preserves its original tag identity; this is already part of the released binary.

Early UI work fixed a message-box leak, subtitle lifetime, animation callback cleanup and an optional pointer guard. Later diagnostics captured `abort` leading to `_exit(1)`. The original dynamic UI event queue had a faulty bound expression and allocated while posting events, including interrupt contexts.

The working queue uses 64 fixed entries with one initialization allocation, interrupt-protected post/pop, callbacks outside the lock, and coalescing of pending redraw/animation events. Full queues increment a drop counter. Preserve this design, the UI cleanup and the player's 4 KB reserve. The exact historical failed allocation was not captured, so these fixes are not a complete causal reconstruction of every reported symptom.

## Catalogue and Preferidos

Seven external files hold 257 entries: Español 48, Català 4, Français 69, Italiano 129, Italiano / English 1, Español Latam 3 and Neutral 3. Each file has a 16-byte `FABACAT1` header and 160-byte records containing a little-endian 16-bit ID and 158 bytes of NUL-padded UTF-8 title. The UI loads ten titles per page.

Preferidos stores numeric IDs rather than copied tags or titles. Two 528-byte files, `/faba/pref0.bin` and `/faba/pref1.bin`, alternate saves. Each contains `FBP1`, a 32-bit generation, 16-bit count, up to 257 sorted unique IDs, padding and an FNV-1a checksum. A save writes the inactive bank and verifies it by readback before updating RAM. Load selects the newest valid generation with wraparound handling.

A corrupt newest bank can fall back to an older valid one. If both banks are absent, the list starts empty. If no valid bank exists and a bank is corrupt or unreadable, the firmware reports unavailable favourites and does not silently overwrite it. An interrupted first-ever save has no earlier valid state to fall back to. Back up both banks before recovery.

## Retained playback diagnostics

After a reproducible reset or unexpected exit, avoid a manual restart or another title selection before capturing the report. Reopen BLE File Transfer and use a fresh directory:

```sh
.build/bin/pixl-read-faba-diag backups/failure-01
python3 tools/decode-faba-diag.py backups/failure-01/report.json \
  --binary evidence/faba10/pixljs.bin
```

Set `PIXL_UUID` to the identified application peripheral first. The binary and ELF must match the installed firmware. A return address near a symbol boundary needs disassembly, not just the nearest symbol name. The archived ELF is `evidence/faba10/pixljs.out`.

The 64-byte retained report includes phase, story, reset reason, last input, heap-at-start and first fault details. It may survive a software reset or DFU, so an old report is not automatically a new failure. Starting another story replaces it. Heap-at-start is not heap-at-failure.

Use `pixl-read-trace`, `decode-faba-trace.py` and `replay-faba-state.py` for NFC traces. The repository's `synthetic.trace` is a host fixture; the Faba and phone traces are the recorded regression inputs.

## Release history

| Release | Main change |
| --- | --- |
| faba1 | NFC recorder |
| faba2 | TIMER2/PPI timing diagnostics |
| faba3 | Scoped early READ-07 compatibility |
| faba4 | First-plus-tail trace retention |
| faba5 | Language catalogue and private playback |
| faba6 | UI lifetime fixes and official logo |
| faba7 | Retained playback diagnostic |
| faba8 | Assert, CPU and reset fault capture |
| faba9 | Bounded interrupt-safe UI queue; playback confirmed |
| faba10 | Persistent Preferidos; restart persistence confirmed |

Changing the UID, trying alternate NTAG215 emulation and installing stock vendor 2.16.1 did not resolve the original compatibility failure. Phone read success, field wake-up and zero read counters were insufficient to diagnose Faba's actual exchange. Native Mac tools provided the reliable development transfer path after browser attempts failed.
