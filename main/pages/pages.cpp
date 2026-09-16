#include "pages/pages.h"

#include <cstring>

#include "ui/layout.h"

namespace pages {

const char *const kIntentions[5] = {"Focus", "Create", "Rest", "Move", "Connect"};
const ArtId kIntentionIcons[5] = {kIconFocus, kIconCreate, kIconRest, kIconMove, kIconConnect};
const char *const kMoodWords[5] = {"Rough", "Low", "Okay", "Good", "Great"};

namespace {

using layout::kMargin;

constexpr int kMoodCell = 120;
constexpr int kMoodRow1Y = 160;
constexpr int kMoodRow2Y = 330;
constexpr int kMoodRow1X[3] = {120, 240, 360};
constexpr int kMoodRow2X[2] = {180, 300};

constexpr int kEnergyBlock = 52;
constexpr int kEnergyGap = 14;
constexpr int kEnergyY = 400;
constexpr int kEnergyX0 = (FrameBuffer::kWidth - (5 * kEnergyBlock + 4 * kEnergyGap)) / 2;

constexpr int kIntentRowH = 88;
constexpr int kIntentRowGap = 12;
constexpr int kIntentY0 = 150;

constexpr int kNoteCardH = 140;
constexpr int kNoteCardGap = 18;
constexpr int kNoteY0 = 110;

const char *const kSampleNotes[3] = {
    "今天学会了新工具，很开心。",
    "下午有点累，想早点休息。",
    "和朋友吃了火锅，满足。",
};
const char *const kSampleTimes[3] = {"SEP 12 10:21", "SEP 11 10:21", "SEP 10 10:21"};

const char *const kMoodIds[5] = {"mood1", "mood2", "mood3", "mood4", "mood5"};
const char *const kEnergyIds[5] = {"energy1", "energy2", "energy3", "energy4", "energy5"};
const char *const kIntentIds[5] = {"intent0", "intent1", "intent2", "intent3", "intent4"};

void mood_cell(int i, int &x, int &y)
{
    if (i < 3) {
        x = kMoodRow1X[i];
        y = kMoodRow1Y;
    } else {
        x = kMoodRow2X[i - 3];
        y = kMoodRow2Y;
    }
}

void status_bar(FrameBuffer &fb, const AppState &s, const Resources &r)
{
    r.font[layout::kLatin18].draw(fb, kMargin, 11, "me.status");
    r.font[layout::kLatin14].draw_right(fb, FrameBuffer::kWidth - kMargin, 14, s.clock);
}

void progress(FrameBuffer &fb, int step)
{
    const int seg_w = (layout::kContentWidth - 2 * 8) / 3;
    for (int i = 0; i < 3; ++i) {
        const int x = kMargin + i * (seg_w + 8);
        if (i < step) {
            fb.fill_rounded_rect(x, 56, seg_w, 8, 4);
        } else {
            fb.draw_rounded_rect(x, 56, seg_w, 8, 4);
        }
    }
}

void back_arrow(FrameBuffer &fb, const Resources &r)
{
    r.art[kIconArrowLeft].draw(fb, kMargin, 758);
}

void hint(FrameBuffer &fb, const Resources &r, const char *text)
{
    r.font[layout::kLatin14].draw_centered(fb, 764, text);
}

void talk_button(FrameBuffer &fb, const Resources &r, int y)
{
    fb.draw_rounded_rect(kMargin, y, layout::kContentWidth, 68, 20);
    const char *label = "Hold to Talk";
    const int group_w = r.art[kIconMic].width() + 12 + r.font[layout::kLatin18].width(label);
    const int gx = (FrameBuffer::kWidth - group_w) / 2;
    r.art[kIconMic].draw(fb, gx, y + 22);
    r.font[layout::kLatin18].draw(fb, gx + r.art[kIconMic].width() + 12, y + 25, label);
}

void render_home(FrameBuffer &fb, const AppState &, const Resources &r)
{
    r.art[kCatHome].draw_centered(fb, 240, 80);
    r.art[kIconHeart].draw(fb, 330, 70);
    r.font[layout::kLatin28].draw_centered(fb, 260, "How are you today?");
    r.font[layout::kLatin16].draw_centered(fb, 302, "A moment for yourself");

    fb.fill_rounded_rect(kMargin, 560, layout::kContentWidth, 76, 20);
    r.font[layout::kLatin24].draw_centered(fb, 586, "CHECK IN", false);

    talk_button(fb, r, 652);

    r.font[layout::kLatin14].draw_centered(fb, 768, "A SMALLER SCROLL. A BRIGHTER LIFE.");
}

void render_mood(FrameBuffer &fb, const AppState &, const Resources &r)
{
    progress(fb, 1);
    r.font[layout::kLatin24].draw_centered(fb, 92, "How are you feeling today?");
    for (int i = 0; i < 5; ++i) {
        int cx, cy;
        mood_cell(i, cx, cy);
        r.art[static_cast<ArtId>(kCatMoodRough + i)].draw_centered(fb, cx, cy);
        r.font[layout::kLatin14].draw_centered_at(fb, cx, cy + 88, kMoodWords[i]);
    }
    back_arrow(fb, r);
    hint(fb, r, "Tap a cat to continue.");
}

void render_energy(FrameBuffer &fb, const AppState &s, const Resources &r)
{
    progress(fb, 2);
    r.font[layout::kLatin24].draw_centered(fb, 92, "How's your energy today?");
    r.art[kCatEnergy].draw_centered(fb, 240, 150);

    const int bx = 240 - 24;
    r.art[kIconBatteryFrame].draw(fb, bx, 300);
    for (int i = 0; i < s.energy && i < 5; ++i) {
        fb.fill_rect(bx + 5 + i * 7, 306, 6, 12);
    }

    for (int i = 0; i < 5; ++i) {
        const int x = kEnergyX0 + i * (kEnergyBlock + kEnergyGap);
        const bool filled = i < s.energy;
        if (filled) {
            fb.fill_rounded_rect(x, kEnergyY, kEnergyBlock, kEnergyBlock, 12);
        } else {
            fb.draw_rounded_rect(x, kEnergyY, kEnergyBlock, kEnergyBlock, 12);
        }
        char digit[2] = {static_cast<char>('1' + i), 0};
        r.font[layout::kLatin18].draw_centered_at(fb, x + kEnergyBlock / 2, kEnergyY + 16, digit, !filled);
    }
    back_arrow(fb, r);
    hint(fb, r, "Tap a block to set energy.");
}

void render_intention(FrameBuffer &fb, const AppState &s, const Resources &r)
{
    progress(fb, 3);
    r.font[layout::kLatin24].draw_centered(fb, 92, "What do you need today?");
    for (int i = 0; i < 5; ++i) {
        const int y = kIntentY0 + i * (kIntentRowH + kIntentRowGap);
        const bool sel = i == s.intention;
        if (sel) {
            fb.fill_rounded_rect(kMargin, y, layout::kContentWidth, kIntentRowH, 18);
        } else {
            fb.draw_rounded_rect(kMargin, y, layout::kContentWidth, kIntentRowH, 18);
        }
        r.art[kIntentionIcons[i]].draw(fb, 48, y + 28, !sel);
        r.font[layout::kLatin20].draw(fb, 100, y + 32, kIntentions[i], !sel);
    }
    back_arrow(fb, r);
    fb.fill_rounded_rect(336, 748, 120, 44, 14);
    r.font[layout::kLatin18].draw_centered_at(fb, 396, 761, "Done", false);
}

void render_completed(FrameBuffer &fb, const AppState &s, const Resources &r)
{
    r.art[kCatDone].draw_centered(fb, 240, 80);
    r.font[layout::kLatin28].draw_centered(fb, 262, "All set!");
    r.font[layout::kLatin16].draw_centered(fb, 304, "Here's your status for today.");

    fb.draw_rounded_rect(48, 360, 384, 190, 18);
    const BitmapFont &f = r.font[layout::kLatin18];
    f.draw(fb, 72, 392, "Feeling");
    f.draw_right(fb, 408, 392, s.mood > 0 ? kMoodWords[s.mood - 1] : "-");
    f.draw(fb, 72, 448, "Energy");
    char energy[8];
    energy[0] = static_cast<char>('0' + s.energy);
    std::memcpy(energy + 1, " / 5", 5);
    f.draw_right(fb, 408, 448, s.energy > 0 ? energy : "-");
    f.draw(fb, 72, 504, "Today I need");
    f.draw_right(fb, 408, 504, s.intention >= 0 ? kIntentions[s.intention] : "-");

    r.font[layout::kLatin16].draw_centered(fb, 590, "Make something fun today.");

    talk_button(fb, r, 690);
}

void render_listening(FrameBuffer &fb, const AppState &, const Resources &r)
{
    r.art[kIconClose].draw(fb, 432, 56);
    r.font[layout::kLatin28].draw_centered(fb, 120, "Listening...");
    r.art[kCatListening].draw_centered(fb, 240, 180);

    const int bars[7] = {6, 14, 22, 30, 22, 14, 6};
    for (int i = 0; i < 7; ++i) {
        const int x = 240 - (7 * 12) / 2 + i * 12;
        fb.fill_rect(x, 356 - bars[i] / 2, 6, bars[i]);
    }

    r.font[layout::kLatin16].draw_centered(fb, 400, "Hold the AI button and speak naturally.");
    fb.draw_rounded_rect(kMargin, 470, layout::kContentWidth, 80, 18);
    r.font[layout::kLatin18].draw_centered(fb, 500, "Let go of the AI key to save");
    r.font[layout::kLatin14].draw_centered(fb, 640, "Today was a good day... I learned something new...");
}

void render_saved(FrameBuffer &fb, const AppState &, const Resources &r)
{
    r.art[kCatSaved].draw_centered(fb, 240, 70);
    r.art[kIconHeart].draw(fb, 330, 60);
    r.font[layout::kLatin24].draw_centered(fb, 240, "Thanks! Your note is saved.");

    fb.draw_rounded_rect(48, 300, 384, 150, 18);
    r.font[layout::kZh16].draw(fb, 72, 330, kSampleNotes[0]);
    r.font[layout::kLatin14].draw(fb, 72, 408, kSampleTimes[0]);

    fb.draw_rounded_rect(kMargin, 640, layout::kContentWidth, 68, 20);
    r.font[layout::kLatin18].draw_centered(fb, 664, "View All Notes");
    r.art[kIconArrowRight].draw(fb, 408, 662);
}

void render_notes(FrameBuffer &fb, const AppState &s, const Resources &r)
{
    r.art[kIconArrowLeft].draw(fb, kMargin, 56);
    r.font[layout::kLatin24].draw(fb, 60, 52, "My Notes");
    for (int i = 0; i < 3; ++i) {
        fb.fill_rect(432 + i * 12, 66, 4, 4);
    }

    const int shown = s.note_count < 3 ? s.note_count : 3;
    for (int i = 0; i < shown; ++i) {
        const int y = kNoteY0 + i * (kNoteCardH + kNoteCardGap);
        fb.draw_rounded_rect(kMargin, y, layout::kContentWidth, kNoteCardH, 18);
        r.font[layout::kLatin14].draw(fb, 48, y + 20, kSampleTimes[i]);
        r.font[layout::kZh16].draw(fb, 48, y + 52, kSampleNotes[i]);
    }

    fb.fill_rounded_rect(kMargin, 660, layout::kContentWidth, 68, 20);
    const int group_w = r.art[kIconPlus].width() + 12 + r.font[layout::kLatin18].width("New Voice Note");
    const int gx = (FrameBuffer::kWidth - group_w) / 2;
    r.art[kIconPlus].draw(fb, gx, 682, false);
    r.font[layout::kLatin18].draw(fb, gx + r.art[kIconPlus].width() + 12, 684, "New Voice Note", false);
}

void add(HitRegion *out, int &n, int max, const char *id, int x, int y, int w, int h)
{
    if (n < max) {
        out[n++] = HitRegion{id, static_cast<int16_t>(x), static_cast<int16_t>(y),
                             static_cast<int16_t>(w), static_cast<int16_t>(h)};
    }
}

void reset_checkin(AppState &s)
{
    s.page = Page::Home;
    s.mood = 0;
    s.energy = 0;
    s.intention = -1;
}

}  // namespace

void render(FrameBuffer &fb, const AppState &state, const Resources &res)
{
    fb.clear();
    status_bar(fb, state, res);
    switch (state.page) {
    case Page::Home: render_home(fb, state, res); break;
    case Page::Mood: render_mood(fb, state, res); break;
    case Page::Energy: render_energy(fb, state, res); break;
    case Page::Intention: render_intention(fb, state, res); break;
    case Page::Completed: render_completed(fb, state, res); break;
    case Page::Listening: render_listening(fb, state, res); break;
    case Page::Saved: render_saved(fb, state, res); break;
    case Page::Notes: render_notes(fb, state, res); break;
    }
}

int regions(const AppState &state, HitRegion *out, int max)
{
    int n = 0;
    switch (state.page) {
    case Page::Home:
        add(out, n, max, "checkin", kMargin, 560, layout::kContentWidth, 76);
        add(out, n, max, "talk", kMargin, 652, layout::kContentWidth, 68);
        break;
    case Page::Mood:
        for (int i = 0; i < 5; ++i) {
            int cx, cy;
            mood_cell(i, cx, cy);
            add(out, n, max, kMoodIds[i], cx - kMoodCell / 2, cy - 10, kMoodCell, kMoodCell);
        }
        add(out, n, max, "back", 12, 746, 48, 48);
        break;
    case Page::Energy:
        for (int i = 0; i < 5; ++i) {
            add(out, n, max, kEnergyIds[i], kEnergyX0 + i * (kEnergyBlock + kEnergyGap), kEnergyY,
                kEnergyBlock, kEnergyBlock);
        }
        add(out, n, max, "back", 12, 746, 48, 48);
        break;
    case Page::Intention:
        for (int i = 0; i < 5; ++i) {
            add(out, n, max, kIntentIds[i], kMargin, kIntentY0 + i * (kIntentRowH + kIntentRowGap),
                layout::kContentWidth, kIntentRowH);
        }
        add(out, n, max, "back", 12, 746, 48, 48);
        add(out, n, max, "done", 336, 748, 120, 44);
        break;
    case Page::Completed:
        add(out, n, max, "talk", kMargin, 690, layout::kContentWidth, 68);
        break;
    case Page::Listening:
        add(out, n, max, "close", 420, 44, 48, 48);
        add(out, n, max, "release", kMargin, 470, layout::kContentWidth, 80);
        break;
    case Page::Saved:
        add(out, n, max, "viewall", kMargin, 640, layout::kContentWidth, 68);
        break;
    case Page::Notes:
        add(out, n, max, "back", 12, 44, 48, 48);
        add(out, n, max, "new", kMargin, 660, layout::kContentWidth, 68);
        break;
    }
    return n;
}

bool on_tap(AppState &s, const char *id)
{
    if (std::strcmp(id, "checkin") == 0) { s.page = Page::Mood; return true; }
    if (std::strcmp(id, "talk") == 0 || std::strcmp(id, "new") == 0) {
        s.mood = 0;
        s.energy = 0;
        s.intention = -1;
        s.page = Page::Listening;
        return true;
    }
    if (std::strncmp(id, "mood", 4) == 0) {
        s.mood = id[4] - '0';
        s.page = Page::Energy;
        return true;
    }
    if (std::strncmp(id, "energy", 6) == 0) {
        s.energy = id[6] - '0';
        s.page = Page::Intention;
        return true;
    }
    if (std::strncmp(id, "intent", 6) == 0) { s.intention = id[6] - '0'; return true; }
    if (std::strcmp(id, "done") == 0) { s.page = Page::Completed; return true; }
    if (std::strcmp(id, "close") == 0) { reset_checkin(s); return true; }
    if (std::strcmp(id, "release") == 0) { s.page = Page::Saved; return true; }
    if (std::strcmp(id, "viewall") == 0) { s.page = Page::Notes; return true; }
    if (std::strcmp(id, "back") == 0) {
        switch (s.page) {
        case Page::Mood: reset_checkin(s); return true;
        case Page::Energy: s.page = Page::Mood; return true;
        case Page::Intention: s.page = Page::Energy; return true;
        case Page::Notes: reset_checkin(s); return true;
        default: return false;
        }
    }
    return false;
}

const char *name(Page page)
{
    switch (page) {
    case Page::Home: return "Home";
    case Page::Mood: return "Mood";
    case Page::Energy: return "Energy";
    case Page::Intention: return "Intention";
    case Page::Completed: return "Completed";
    case Page::Listening: return "Listening";
    case Page::Saved: return "Saved";
    case Page::Notes: return "Notes";
    }
    return "?";
}

}  // namespace pages
