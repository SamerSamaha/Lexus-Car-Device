# Latency measurements

Raw rows from `lexus-head-unit --latency-log` (`signal,sample_ms,arrival_ms,frame_ms`, one per sample), committed unchanged, and the output of `tools/measure/measure_latency.py` beside each file. Input to REQ-009: sample to screen at the 95th percentile over 1,000 or more samples on the Pi.

| File | Date | Where | Source | Samples | p95 sample to screen | Notes |
|---|---|---|---|---|---|---|
| none yet | | | | | | |

No measurement on the Pi has been taken yet (`docs/release/PI_BRINGUP_CHECKLIST.md` step 3.9). Desk figures for the D-Bus hop alone are in `docs/design/DN-022-vehicle-data-service.md` and are not REQ-009 evidence.
