// Host unit test for the record read/write layer. Zero ESP-IDF: compiles with
// plain g++ and exercises the exact codec + in-memory store the firmware uses,
// plus a temp-file roundtrip of the on-disk JSONL format.
//
// Run: bash tests/run_tests.sh
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "app/record_store.h"

using namespace store;

static int g_fail = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
            ++g_fail;                                                      \
        }                                                                  \
    } while (0)

static void test_codec_roundtrip()
{
    printf("[codec] roundtrip\n");
    Record r;
    r.ts = 1758067200;
    r.mood = 4;
    r.energy = 3;
    r.intention = 2;
    std::strcpy(r.text, "今天心情不错，学到了新东西");

    char line[512];
    const int n = record_to_jsonl(r, line, sizeof line);
    CHECK(n > 0);

    Record back;
    CHECK(record_from_jsonl(line, &back));
    CHECK(back.ts == r.ts);
    CHECK(back.mood == r.mood);
    CHECK(back.energy == r.energy);
    CHECK(back.intention == r.intention);
    CHECK(std::strcmp(back.text, r.text) == 0);
    CHECK(back.is_note());
}

static void test_codec_escapes()
{
    printf("[codec] escaping quotes/backslash/newline\n");
    Record r;
    r.ts = 1;
    r.mood = 1;
    r.energy = 5;
    r.intention = 0;
    std::strcpy(r.text, "a\"b\\c\nd");

    char line[512];
    CHECK(record_to_jsonl(r, line, sizeof line) > 0);
    // The raw line must not contain an unescaped quote inside the text value or
    // a literal newline (which would break JSONL framing).
    CHECK(std::strchr(line, '\n') == nullptr);

    Record back;
    CHECK(record_from_jsonl(line, &back));
    CHECK(std::strcmp(back.text, "a\"b\\c\nd") == 0);
}

static void test_checkin_has_empty_text()
{
    printf("[codec] pure check-in (no note)\n");
    Record r;
    r.ts = 99;
    r.mood = 5;
    r.energy = 4;
    r.intention = 1;
    char line[512];
    CHECK(record_to_jsonl(r, line, sizeof line) > 0);
    Record back;
    CHECK(record_from_jsonl(line, &back));
    CHECK(!back.is_note());
    CHECK(back.text[0] == '\0');
}

static void test_parse_tolerance()
{
    printf("[codec] tolerant parse: missing keys / reordered\n");
    Record r;
    // text before ts, no intention key at all
    CHECK(record_from_jsonl("{\"text\":\"hi\",\"mood\":3,\"ts\":42}", &r));
    CHECK(r.ts == 42);
    CHECK(r.mood == 3);
    CHECK(r.intention == -1);  // default when absent
    CHECK(std::strcmp(r.text, "hi") == 0);

    // malformed line
    Record bad;
    CHECK(!record_from_jsonl("not json at all", &bad));
}

static void test_memory_store()
{
    printf("[memory store] append / count / recent / within / clear\n");
    MemoryRecordStore s(8);
    CHECK(s.count() == 0);
    CHECK(s.load_recent(nullptr, 0) == 0);

    const int64_t day = 86400;
    const int64_t now = 1000 * day;
    // Append in chronological order (oldest first), exactly as the device does
    // as time passes: i=0 is the oldest (now-4d), i=4 the newest (now).
    for (int i = 0; i < 5; ++i) {
        Record r;
        r.ts = now - (4 - i) * day;
        r.mood = i + 1;
        char txt[32];
        std::snprintf(txt, sizeof txt, "note %d", i);
        std::strcpy(r.text, txt);
        CHECK(s.append(r));
    }
    CHECK(s.count() == 5);

    Record out[8];
    const int got = s.load_recent(out, 3);
    CHECK(got == 3);
    // newest first: ts should be now, now-day, now-2day
    CHECK(out[0].ts == now);
    CHECK(out[1].ts == now - day);
    CHECK(out[2].ts == now - 2 * day);
    CHECK(std::strcmp(out[0].text, "note 4") == 0);

    // Window is inclusive of the cutoff: last 3 days = {now-3d..now} = 4 records.
    CHECK(s.count_within(now, 3) == 4);
    // last 10 days = all 5
    CHECK(s.count_within(now, 10) == 5);
    // same day only = the single newest record
    CHECK(s.count_within(now, 0) == 1);

    CHECK(s.clear());
    CHECK(s.count() == 0);
}

static void test_file_roundtrip()
{
    printf("[file] JSONL append + read back (on-disk format)\n");
    const char *path = "/tmp/sticky_record_test.jsonl";
    std::remove(path);

    // Append two records exactly the way FatfsRecordStore does (line + '\n').
    FILE *f = std::fopen(path, "ab");
    CHECK(f != nullptr);
    if (f) {
        for (int i = 0; i < 2; ++i) {
            Record r;
            r.ts = 100 + i;
            r.mood = 3 + i;
            r.energy = 2;
            r.intention = i;
            std::strcpy(r.text, i == 0 ? "第一条" : "second line");
            char line[512];
            int n = record_to_jsonl(r, line, sizeof line);
            line[n] = '\n';
            std::fwrite(line, 1, n + 1, f);
        }
        std::fclose(f);
    }

    // Read back.
    f = std::fopen(path, "rb");
    CHECK(f != nullptr);
    int count = 0;
    Record last;
    if (f) {
        char line[512];
        while (std::fgets(line, sizeof line, f) != nullptr) {
            Record r;
            if (record_from_jsonl(line, &r)) {
                last = r;
                ++count;
            }
        }
        std::fclose(f);
    }
    CHECK(count == 2);
    CHECK(last.ts == 101);
    CHECK(std::strcmp(last.text, "second line") == 0);
    std::remove(path);
}

int main()
{
    test_codec_roundtrip();
    test_codec_escapes();
    test_checkin_has_empty_text();
    test_parse_tolerance();
    test_memory_store();
    test_file_roundtrip();

    if (g_fail == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d CHECK(S) FAILED\n", g_fail);
    return 1;
}
