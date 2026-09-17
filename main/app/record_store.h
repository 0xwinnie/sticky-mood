#pragma once

#include <cstdint>

// Record store: the read/write layer for mood check-ins and voice notes.
//
// This header is deliberately ZERO ESP-IDF so the host simulator and the unit
// tests compile the exact same code that runs on the device (same rule as
// main/ui, main/pages, main/app/app_state.h). The persistent backend lives in
// main/board/fatfs_record_store.cpp; this file only defines the data model, the
// on-disk line format, the abstract interface, and an in-memory backend.
//
// Storage policy (docs/REQUIREMENTS.md §2e):
//   D13 - internal flash is the primary store; this layer never touches the SD.
//   D14 - append() is silent on success; it returns false ONLY on failure so the
//         caller can do a partial refresh and show one failure line.
//   D15 - no auto-rotation. clear() is the user-initiated export-and-wipe.
namespace store {

// Capacity for the transcribed note text, in bytes (UTF-8). A voice note is a
// short spoken sentence; 160 bytes holds ~50 Chinese characters, plenty.
constexpr int kMaxTextBytes = 160;

// One journal entry. A check-in fills mood/energy/intention and leaves text
// empty; a voice note fills text (and may carry the day's mood too).
struct Record {
    int64_t ts = 0;          // Unix epoch seconds (UTC). 0 = unknown timestamp.
    int mood = 0;            // 1..5, 0 = none
    int energy = 0;          // 1..5, 0 = none
    int intention = -1;      // 0..4 index into pages::kIntentions, -1 = none
    char text[kMaxTextBytes] = {};  // UTF-8 note body, "" for a pure check-in

    bool is_note() const { return text[0] != '\0'; }
};

// --- on-disk line format ---------------------------------------------------
// One JSON object per line (JSONL), append-friendly and easy to filter by day:
//   {"ts":1758067200,"mood":4,"energy":3,"intention":0,"text":"..."}
// Hand-rolled (not cJSON) to keep this file free of ESP-IDF dependencies.

// Serialize r to a single line WITHOUT a trailing newline. Returns the number
// of bytes written (excluding NUL), or -1 if out_len is too small.
int record_to_jsonl(const Record &r, char *out, int out_len);

// Parse one JSONL line back into out. Tolerant of missing keys and key order.
// Returns true on success, false if the line is malformed.
bool record_from_jsonl(const char *line, Record *out);

// --- abstract backend ------------------------------------------------------
class RecordStore {
public:
    virtual ~RecordStore() = default;

    // Append one record. Silent on success. Returns false ONLY on a real write
    // failure (D14) - the caller then shows a partial-refresh failure line.
    virtual bool append(const Record &r) = 0;

    // Load up to `max` most-recent records into `out`, NEWEST FIRST. Returns the
    // number written (<= max). Used by the Notes page.
    virtual int load_recent(Record *out, int max) = 0;

    // Total number of stored records (0 if none / unavailable).
    virtual int count() = 0;

    // Records whose ts falls within the last `days` days relative to `now_ts`.
    // Powers the "recent three months" view without deleting older data (D5).
    virtual int count_within(int64_t now_ts, int days) = 0;

    // Delete everything. User-initiated export-and-clear (D15). Returns true on
    // success.
    virtual bool clear() = 0;

    // Free bytes on the backing store, or -1 if unknown. Lets the UI warn before
    // the 4 MB partition fills.
    virtual int64_t free_bytes() = 0;
};

// --- in-memory backend -----------------------------------------------------
// Zero-IDF. Used by the host unit tests, by the web simulator, and on-device as
// a graceful fallback if the FATFS partition fails to mount (so the app still
// runs, it just won't persist across reboots).
class MemoryRecordStore : public RecordStore {
public:
    explicit MemoryRecordStore(int capacity = 256);
    ~MemoryRecordStore() override;

    bool append(const Record &r) override;
    int load_recent(Record *out, int max) override;
    int count() override;
    int count_within(int64_t now_ts, int days) override;
    bool clear() override;
    int64_t free_bytes() override;

private:
    Record *buf_;
    int cap_;
    int n_;
};

}  // namespace store
