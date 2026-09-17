#include "app/record_store.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace store {
namespace {

// Append a JSON-escaped copy of `s` (UTF-8) to `out`. Raw multi-byte UTF-8 is
// valid inside a JSON string, so Chinese text passes through untouched; only
// the structural characters and ASCII control codes need escaping.
void json_escape_into(char *out, int out_len, int *pos, const char *s)
{
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); *p != '\0'; ++p) {
        const char c = static_cast<char>(*p);
        const char *rep = nullptr;
        char hexbuf[8];
        switch (c) {
        case '"': rep = "\\\""; break;
        case '\\': rep = "\\\\"; break;
        case '\n': rep = "\\n"; break;
        case '\r': rep = "\\r"; break;
        case '\t': rep = "\\t"; break;
        default:
            if (*p < 0x20) {
                snprintf(hexbuf, sizeof hexbuf, "\\u%04x", *p);
                rep = hexbuf;
            }
            break;
        }
        if (rep != nullptr) {
            for (const char *q = rep; *q != '\0'; ++q) {
                if (*pos < out_len - 1) {
                    out[(*pos)++] = *q;
                }
            }
        } else {
            if (*pos < out_len - 1) {
                out[(*pos)++] = c;
            }
        }
    }
}

// Find the value substring for a given "key": in a flat JSON object. Returns a
// pointer just past the colon (leading spaces skipped), or nullptr if absent.
const char *find_value(const char *json, const char *key)
{
    char pat[24];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (p == nullptr) {
        return nullptr;
    }
    p += strlen(pat);
    while (*p == ' ' || *p == ':') {
        ++p;
    }
    return p;
}

// Read the quoted string starting at `p` (which must point at the opening '"'),
// unescaping into dst. Returns true on success.
bool read_json_string(const char *p, char *dst, int dst_len)
{
    if (p == nullptr || *p != '"') {
        return false;
    }
    ++p;
    int n = 0;
    while (*p != '\0' && *p != '"') {
        char c = *p;
        if (c == '\\') {
            ++p;
            switch (*p) {
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'u': {
                // \uXXXX: only the BMP-ASCII range is expected (control codes);
                // decode it as a byte if small, otherwise keep it literal.
                unsigned int code = 0;
                for (int i = 1; i <= 4 && p[i] != '\0'; ++i) {
                    const char h = p[i];
                    code <<= 4;
                    if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
                }
                p += 4;
                c = (code < 0x80) ? static_cast<char>(code) : '?';
                break;
            }
            default: c = *p; break;
            }
            if (*p == '\0') {
                break;
            }
            ++p;
        } else {
            ++p;
        }
        if (n < dst_len - 1) {
            dst[n++] = c;
        }
    }
    dst[n] = '\0';
    return true;
}

long long read_int(const char *p, long long fallback)
{
    if (p == nullptr) {
        return fallback;
    }
    char *end = nullptr;
    const long long v = strtoll(p, &end, 10);
    return (end == p) ? fallback : v;
}

}  // namespace

int record_to_jsonl(const Record &r, char *out, int out_len)
{
    if (out == nullptr || out_len < 32) {
        return -1;
    }
    int pos = 0;
    pos += snprintf(out + pos, out_len - pos,
                    "{\"ts\":%lld,\"mood\":%d,\"energy\":%d,\"intention\":%d,\"text\":\"",
                    (long long)r.ts, r.mood, r.energy, r.intention);
    if (pos < 0 || pos >= out_len - 1) {
        return -1;
    }
    json_escape_into(out, out_len, &pos, r.text);
    if (pos < out_len - 2) {
        out[pos++] = '"';
        out[pos++] = '}';
    } else {
        return -1;
    }
    out[pos] = '\0';
    return pos;
}

bool record_from_jsonl(const char *line, Record *out)
{
    if (line == nullptr || out == nullptr) {
        return false;
    }
    if (strchr(line, '{') == nullptr) {
        return false;
    }
    Record r;
    r.ts = read_int(find_value(line, "ts"), 0);
    r.mood = (int)read_int(find_value(line, "mood"), 0);
    r.energy = (int)read_int(find_value(line, "energy"), 0);
    r.intention = (int)read_int(find_value(line, "intention"), -1);
    if (!read_json_string(find_value(line, "text"), r.text, sizeof r.text)) {
        r.text[0] = '\0';
    }
    *out = r;
    return true;
}

// --- MemoryRecordStore -----------------------------------------------------

MemoryRecordStore::MemoryRecordStore(int capacity)
    : cap_(capacity > 0 ? capacity : 1), n_(0)
{
    buf_ = new Record[cap_];
}

MemoryRecordStore::~MemoryRecordStore()
{
    delete[] buf_;
}

bool MemoryRecordStore::append(const Record &r)
{
    if (n_ < cap_) {
        buf_[n_++] = r;
        return true;
    }
    // Full: drop the oldest, shift down. D15 says no silent rotation of the
    // *persistent* store; the in-memory fallback is bounded by design and the
    // device path uses FatfsRecordStore, which reports false instead.
    for (int i = 1; i < cap_; ++i) {
        buf_[i - 1] = buf_[i];
    }
    buf_[cap_ - 1] = r;
    return true;
}

int MemoryRecordStore::load_recent(Record *out, int max)
{
    if (out == nullptr || max <= 0) {
        return 0;
    }
    int k = n_ < max ? n_ : max;
    for (int i = 0; i < k; ++i) {
        out[i] = buf_[n_ - 1 - i];  // newest first
    }
    return k;
}

int MemoryRecordStore::count()
{
    return n_;
}

int MemoryRecordStore::count_within(int64_t now_ts, int days)
{
    const int64_t cutoff = now_ts - (int64_t)days * 86400;
    int c = 0;
    for (int i = 0; i < n_; ++i) {
        if (buf_[i].ts >= cutoff && buf_[i].ts <= now_ts) {
            ++c;
        }
    }
    return c;
}

bool MemoryRecordStore::clear()
{
    n_ = 0;
    return true;
}

int64_t MemoryRecordStore::free_bytes()
{
    // Not a real filesystem; report a nominal "plenty" so callers don't warn.
    return (int64_t)(cap_ - n_) * 128;
}

}  // namespace store
