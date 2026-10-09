# Memory measurements

Raw samples from `tools/measure/sample_process_memory.py` (RSS and PSS of a process and all its descendants, one row per interval), committed unchanged with their `.summary.txt`. PSS is the figure to compare: it divides shared pages among the processes that map them, so the sum over a multi-process browser is not inflated as the RSS sum is.

Inputs to REQ-018 (browser memory during video playback, 10 s for 10 min) and REQ-014 (per-process budgets, LHU-032).

| File | Date | What ran | Samples | PSS median | Notes |
|---|---|---|---|---|---|
| none yet | | | | | |

No measurement has been taken yet: the browser runs on the Pi only (`docs/test/MANUAL_WEB_APPS_PROCEDURE.md`).
