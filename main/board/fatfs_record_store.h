#pragma once

#include "app/record_store.h"

namespace board {

// FATFS-backed RecordStore. Persists records as JSONL in
//   <storage_mount_point()>/records.jsonl
// on the internal-flash "storage" partition (D13) - never the SD card.
//
// Uses only POSIX file I/O through the ESP-IDF VFS, so the same code path the
// host unit test exercises against a temp dir is what runs on the device.
class FatfsRecordStore : public store::RecordStore {
public:
    FatfsRecordStore();  // binds to board::storage_mount_point()

    bool append(const store::Record &r) override;
    int load_recent(store::Record *out, int max) override;
    int count() override;
    int count_within(int64_t now_ts, int days) override;
    bool clear() override;
    int64_t free_bytes() override;

    // Full path of the log file, for diagnostics.
    const char *path() const { return path_; }

private:
    char path_[40];
};

}  // namespace board
