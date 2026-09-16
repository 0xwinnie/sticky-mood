#pragma once

#include <cstdint>

#include "app/app_state.h"
#include "ui/framebuffer.h"
#include "ui/resources.h"

// A tappable rectangle. The firmware hit-tests GT911 points against these;
// the simulator turns them into clickable HTML areas. ids are stable strings
// consumed by pages::on_tap.
struct HitRegion {
    const char *id;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

namespace pages {

constexpr int kMaxRegions = 12;

extern const char *const kIntentions[5];
extern const ArtId kIntentionIcons[5];
extern const char *const kMoodWords[5];

void render(FrameBuffer &fb, const AppState &state, const Resources &res);
int regions(const AppState &state, HitRegion *out, int max);
bool on_tap(AppState &state, const char *id);
const char *name(Page page);

}  // namespace pages
