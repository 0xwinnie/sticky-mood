#include "board/fatfs_record_store.h"

#include <cstdio>
#include <cstring>
#include <unistd.h>  // fsync

#include "board/storage.h"
#include "esp_log.h"

namespace board {
namespace {

constexpr const char *kTag = "record_store";
constexpr const char *kFileName = "/records.jsonl";
// A serialized line is bounded by Record's fields; 512 B is generous.
constexpr int kLineBuf = 512;

}  // namespace

FatfsRecordStore::FatfsRecordStore()
{
    snprintf(path_, sizeof path_, "%s%s", storage_mount_point(), kFileName);
}

bool FatfsRecordStore::append(const store::Record &r)
{
    if (!storage_ready()) {
        ESP_LOGE(kTag, "append failed: storage not mounted");
        return false;
    }
    char line[kLineBuf];
    const int n = store::record_to_jsonl(r, line, sizeof line);
    if (n <= 0) {
        ESP_LOGE(kTag, "append failed: serialize error");
        return false;
    }

    FILE *f = fopen(path_, "ab");
    if (f == nullptr) {
        ESP_LOGE(kTag, "append failed: cannot open %s", path_);
        return false;
    }
    line[n] = '\n';
    const size_t wrote = fwrite(line, 1, (size_t)n + 1, f);
    fflush(f);
    fsync(fileno(f));  // commit to flash before the device may sleep / lose power
    fclose(f);

    if (wrote != (size_t)n + 1) {
        ESP_LOGE(kTag, "append failed: short write (%u/%d)", (unsigned)wrote, n + 1);
        return false;
    }
    // D14: success is silent.
    return true;
}

int FatfsRecordStore::load_recent(store::Record *out, int max)
{
    if (out == nullptr || max <= 0 || !storage_ready()) {
        return 0;
    }
    FILE *f = fopen(path_, "rb");
    if (f == nullptr) {
        return 0;  // no file yet = no records
    }

    // Keep only the last `max` parsed records in a ring, then emit newest-first.
    store::Record *ring = new store::Record[max];
    int ring_n = 0;   // number of valid slots (<= max)
    int ring_pos = 0; // next write index once full

    char line[kLineBuf];
    while (fgets(line, sizeof line, f) != nullptr) {
        if (line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        store::Record r;
        if (!store::record_from_jsonl(line, &r)) {
            continue;
        }
        if (ring_n < max) {
            ring[ring_n++] = r;
        } else {
            ring[ring_pos] = r;
            ring_pos = (ring_pos + 1) % max;
        }
    }
    fclose(f);

    // ring_pos points at the oldest entry when the ring is full.
    for (int i = 0; i < ring_n; ++i) {
        const int idx = (ring_pos + i) % ring_n;  // oldest -> newest
        out[ring_n - 1 - i] = ring[idx];          // reverse: newest first
    }
    delete[] ring;
    return ring_n;
}

int FatfsRecordStore::count()
{
    if (!storage_ready()) {
        return 0;
    }
    FILE *f = fopen(path_, "rb");
    if (f == nullptr) {
        return 0;
    }
    int n = 0;
    char line[kLineBuf];
    while (fgets(line, sizeof line, f) != nullptr) {
        if (line[0] != '\n' && line[0] != '\0') {
            ++n;
        }
    }
    fclose(f);
    return n;
}

int FatfsRecordStore::count_within(int64_t now_ts, int days)
{
    if (!storage_ready()) {
        return 0;
    }
    FILE *f = fopen(path_, "rb");
    if (f == nullptr) {
        return 0;
    }
    const int64_t cutoff = now_ts - (int64_t)days * 86400;
    int n = 0;
    char line[kLineBuf];
    while (fgets(line, sizeof line, f) != nullptr) {
        store::Record r;
        if (store::record_from_jsonl(line, &r) && r.ts >= cutoff && r.ts <= now_ts) {
            ++n;
        }
    }
    fclose(f);
    return n;
}

bool FatfsRecordStore::clear()
{
    if (!storage_ready()) {
        return false;
    }
    // Truncate rather than unlink so the inode/path stays valid.
    FILE *f = fopen(path_, "wb");
    if (f == nullptr) {
        ESP_LOGE(kTag, "clear failed: cannot truncate %s", path_);
        return false;
    }
    fclose(f);
    ESP_LOGI(kTag, "cleared %s (user-initiated export-and-clear)", path_);
    return true;
}

int64_t FatfsRecordStore::free_bytes()
{
    return storage_ready() ? (int64_t)storage_free_bytes() : -1;
}

}  // namespace board
