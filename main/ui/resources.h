#pragma once

#include "ui/art.h"
#include "ui/bitmap_font.h"
#include "ui/layout.h"

enum ArtId : uint8_t {
    kCatHome,
    kCatMoodRough,
    kCatMoodLow,
    kCatMoodOkay,
    kCatMoodGood,
    kCatMoodGreat,
    kCatEnergy,
    kCatDone,
    kCatListening,
    kCatSaved,
    kCatLogo,
    kIconFocus,
    kIconCreate,
    kIconRest,
    kIconMove,
    kIconConnect,
    kIconMic,
    kIconBatteryFrame,
    kIconHeart,
    kIconArrowLeft,
    kIconArrowRight,
    kIconPlus,
    kIconClose,
    kArtCount,
};

// All loaded faces and sprites. The simulator fills this from build/font_data
// and build/art_data; the firmware will fill it from embedded blobs.
struct Resources {
    BitmapFont font[layout::kFaceCount];
    Art art[kArtCount];
};

// File stems (without extension) in load order, used by the simulator loader.
extern const char *const kFontNames[layout::kFaceCount];
extern const char *const kArtNames[kArtCount];
