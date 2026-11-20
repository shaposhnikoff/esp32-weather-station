# Future work

These items are a backlog, not part of the current weather-station release.
The user requested tracking OTA on 2026-11-20; implementation has not started.

- [ ] Add Wi-Fi OTA updates for `firmware.bin`.
  - Prepare `ota_0`, `ota_1` and `otadata` partitions while preserving the
    external configuration/cache contract.
  - Install the OTA-capable baseline and its partition table over USB once.
  - Authenticate updates, verify the image and enable rollback after a failed
    first boot.
  - Retain the validated 1024x600 panel parameters, DIO bootloader header and
    shared LVGL configuration.
  - Test interrupted transfers, wrong images, reboot validation and rollback.
  - Document that application OTA does not replace the bootloader or partition
    table through the normal update path.
- [ ] Consider uploading the complete external `config.json` over the network.
  - Keep the single-file source of configuration and apply changes on restart.
  - Validate the complete file and protect credentials in transit and logs.
  - Do not introduce a settings screen, per-setting commands or a second
    configuration store.
