# Credits and licensing

The firmware derives from [solosky/pixl.js](https://github.com/solosky/pixl.js), commit `7a07ab957743d04850d86b55089ea4894ab8fef5`. The root [LICENSE](LICENSE) preserves its GPL version 2 licence text. Firmware modifications follow that licence. Individual dependencies retain their own licence files and notices; the root licence does not replace them.

The Chameleon source starts at `ffb0ccbce9b956ee7ee0afc8329bd588ff37b85c` from solosky/ChameleonUltra, with the local Faba compatibility changes. All dependency revisions are recorded in [source provenance](evidence/source-provenance.json). Dependencies are vendored as ordinary source directories.

The catalogue comes from [wansors/myfaba-hacks](https://github.com/wansors/myfaba-hacks), pinned at `b9f894d294c4a2e2e7a85bfba983959a014c937c`. [Catalogue provenance](catalog/source.json) records the exact input. The firmware does not include story audio.

The FABA logo inputs came from FABA's website. [Asset provenance](assets/faba/source.json) records their URLs and hashes. FABA names and logos belong to their respective owners. This is an independent project; inclusion of a logo is not an endorsement or a grant of rights in that branding.

The Mac updater uses Nordic Semiconductor's IOS-DFU-Library 4.17.0, commit `9c87e9fbce1980487373c2330f066612dad5ba82`, and ZIPFoundation 0.9.20 at `22787ffb59de99e5dc1fbfe80b19c97a904ad48d`. Setup fetches these dependencies with their licence notices. The retained compatibility patch removes only an unused documentation plugin dependency.

Personal device backups, private signing key files, credentials and unrelated development downloads are not part of this repository. Test fixtures contain the NFC exchanges needed for the state-machine regression checks; the playback template preserves the working tag identity used by the released firmware.
