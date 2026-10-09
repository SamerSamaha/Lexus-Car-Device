# Boot-time measurements

`boots.csv` collects one row per cold boot from `tools/measure/measure_boot_time.py record` (the hub's first frame in milliseconds since the kernel started, and `systemd-analyze` kernel and userspace times); `measure_boot_time.py summarise` reports every boot. Input to REQ-013 (provisional target 15 s, 10 or more boots). Firmware and bootloader time before the kernel is measured apart and reported beside.

No boot has been measured yet (`docs/release/PI_BRINGUP_CHECKLIST.md` step 3.9).
