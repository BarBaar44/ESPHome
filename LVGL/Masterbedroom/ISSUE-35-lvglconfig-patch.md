# Superseded — see `ISSUE-40-lvglconfig-patch.md`

This file described a **"Sync HA"** button (`btn_sync_settings`) for issue #35.

**Do not apply it.** #35 was closed as mis-diagnosed: it assumed the failure
mode was *drift* (two values disagreeing) when the real incident was *absence*
(HA helpers deleted by a database corruption, with the panel silently showing
plausible YAML defaults in their place). A button that re-copies HA's value
does nothing useful when there is no value to copy — and worse, appears to
succeed.

The replacement is issue #40: a **"Check HA"** button (`btn_check_settings`)
that reports which helpers are missing, backed by a periodic health check and
an HA problem sensor.

The widget ids here no longer match `lvglsensors.yml` and **will not compile**.

👉 **Use [`ISSUE-40-lvglconfig-patch.md`](./ISSUE-40-lvglconfig-patch.md)
instead.** Delete both files once the patch is applied.
