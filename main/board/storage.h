#ifndef BOARD_STORAGE_H
#define BOARD_STORAGE_H

namespace board {

// Mounts the 4 MB "storage" FATFS partition (wear-levelled) at kMountPoint.
// This is the PRIMARY mood-record store: it lives in internal flash, so the
// journal works with no SD card inserted and never contends with the e-paper
// panel on SPI2. Returns true on success. Idempotent: a second call is a no-op
// that reports the already-mounted state.
bool storage_init();

// Unmounts the storage partition. Mainly for a clean shutdown path / tests.
void storage_deinit();

// True once storage_init() has successfully mounted the partition.
bool storage_ready();

// Absolute VFS path the records live under, e.g. "/data".
const char *storage_mount_point();

// Bytes free / total on the storage partition (0 if not mounted). Useful for
// the "4 MB full -> ask the user to export and clear" policy.
unsigned long long storage_free_bytes();
unsigned long long storage_total_bytes();

}  // namespace board

#endif  // BOARD_STORAGE_H
