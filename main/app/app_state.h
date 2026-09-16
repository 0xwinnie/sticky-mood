#pragma once

#include <cstdint>

// Everything the pages need to know about the current session. Pure data so
// the host simulator and the firmware share the same state machine.
enum class Page : uint8_t {
    Home,
    Mood,
    Energy,
    Intention,
    Completed,
    Listening,
    Saved,
    Notes,
};

struct AppState {
    Page page = Page::Home;
    int mood = 0;       // 1..5, 0 = not chosen yet
    int energy = 0;     // 1..5, 0 = not chosen yet
    int intention = -1;  // 0..4 index into pages::kIntentions, -1 = none
    int note_count = 3; // notes shown on the Saved/Notes pages (SD-backed later)
    char clock[13] = "SEP 15 08:30";
};
