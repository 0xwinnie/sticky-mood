#include "app/device_resources.h"

#include "esp_log.h"

namespace {

constexpr const char *kTag = "device_resources";

#define DECLARE_BLOB(symbol)                                    \
    extern "C" const uint8_t _binary_##symbol##_bin_start[];    \
    extern "C" const uint8_t _binary_##symbol##_bin_end[];

// Order must match kFontNames in ui/resources.cpp.
DECLARE_BLOB(font_latin14)
DECLARE_BLOB(font_latin16)
DECLARE_BLOB(font_latin18)
DECLARE_BLOB(font_latin20)
DECLARE_BLOB(font_latin24)
DECLARE_BLOB(font_latin28)
DECLARE_BLOB(font_zh16)
DECLARE_BLOB(font_zh18)

// Order must match kArtNames in ui/resources.cpp (dashes become underscores).
DECLARE_BLOB(cat_home)
DECLARE_BLOB(cat_mood_rough)
DECLARE_BLOB(cat_mood_low)
DECLARE_BLOB(cat_mood_okay)
DECLARE_BLOB(cat_mood_good)
DECLARE_BLOB(cat_mood_great)
DECLARE_BLOB(cat_energy)
DECLARE_BLOB(cat_done)
DECLARE_BLOB(cat_listening)
DECLARE_BLOB(cat_saved)
DECLARE_BLOB(cat_logo)
DECLARE_BLOB(icon_focus)
DECLARE_BLOB(icon_create)
DECLARE_BLOB(icon_rest)
DECLARE_BLOB(icon_move)
DECLARE_BLOB(icon_connect)
DECLARE_BLOB(icon_mic)
DECLARE_BLOB(icon_battery_frame)
DECLARE_BLOB(icon_heart)
DECLARE_BLOB(icon_arrow_left)
DECLARE_BLOB(icon_arrow_right)
DECLARE_BLOB(icon_plus)
DECLARE_BLOB(icon_close)

struct Blob {
    const uint8_t *start;
    const uint8_t *end;
};

#define BLOB_ENTRY(symbol) {_binary_##symbol##_bin_start, _binary_##symbol##_bin_end}

const Blob kFontBlobs[layout::kFaceCount] = {
    BLOB_ENTRY(font_latin14), BLOB_ENTRY(font_latin16), BLOB_ENTRY(font_latin18),
    BLOB_ENTRY(font_latin20), BLOB_ENTRY(font_latin24), BLOB_ENTRY(font_latin28),
    BLOB_ENTRY(font_zh16),    BLOB_ENTRY(font_zh18),
};

const Blob kArtBlobs[kArtCount] = {
    BLOB_ENTRY(cat_home),       BLOB_ENTRY(cat_mood_rough), BLOB_ENTRY(cat_mood_low),
    BLOB_ENTRY(cat_mood_okay),  BLOB_ENTRY(cat_mood_good),  BLOB_ENTRY(cat_mood_great),
    BLOB_ENTRY(cat_energy),     BLOB_ENTRY(cat_done),       BLOB_ENTRY(cat_listening),
    BLOB_ENTRY(cat_saved),      BLOB_ENTRY(cat_logo),       BLOB_ENTRY(icon_focus),
    BLOB_ENTRY(icon_create),    BLOB_ENTRY(icon_rest),      BLOB_ENTRY(icon_move),
    BLOB_ENTRY(icon_connect),   BLOB_ENTRY(icon_mic),       BLOB_ENTRY(icon_battery_frame),
    BLOB_ENTRY(icon_heart),     BLOB_ENTRY(icon_arrow_left), BLOB_ENTRY(icon_arrow_right),
    BLOB_ENTRY(icon_plus),      BLOB_ENTRY(icon_close),
};

}  // namespace

bool load_embedded_resources(Resources &out)
{
    for (int i = 0; i < layout::kFaceCount; ++i) {
        if (!out.font[i].load(kFontBlobs[i].start, kFontBlobs[i].end - kFontBlobs[i].start)) {
            ESP_LOGE(kTag, "font face %d (%s) failed to load", i, kFontNames[i]);
            return false;
        }
    }
    for (int i = 0; i < kArtCount; ++i) {
        if (!out.art[i].load(kArtBlobs[i].start, kArtBlobs[i].end - kArtBlobs[i].start)) {
            ESP_LOGE(kTag, "sprite %d (%s) failed to load", i, kArtNames[i]);
            return false;
        }
    }
    return true;
}
