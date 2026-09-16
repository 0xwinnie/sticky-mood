// Host simulator: renders every reachable page state with the exact same
// ui/pages code the firmware runs, dumps 1-bit frames and the tap graph for
// tools/sim/make_preview.py to turn into a clickable web preview.
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "app/app_state.h"
#include "pages/pages.h"
#include "ui/framebuffer.h"
#include "ui/resources.h"

namespace {

std::string root;

bool read_file(const std::string &path, std::vector<uint8_t> &out)
{
    FILE *f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        std::fprintf(stderr, "missing %s\n", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(static_cast<size_t>(n));
    const bool ok = std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

// The full choice tuple, not just what the current page draws: Completed shows
// mood/energy/intention together, so deduping on the visible fields alone would
// let a tap walk into a state rendered from a different path's choices.
std::string key(const AppState &s)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d/%d/%d/%d", static_cast<int>(s.page), s.mood, s.energy,
                  s.intention);
    return buf;
}

// Render every reachable state. Truncating here hides taps whose target falls
// past the cap, which broke "done" on deep choice paths; the graph is only a
// few hundred states and the preview is local-only.
constexpr size_t kMaxRendered = 100000;

}  // namespace

int main(int argc, char **argv)
{
    root = argc > 1 ? argv[1] : ".";
    const std::string font_dir = root + "/build/font_data/";
    const std::string art_dir = root + "/build/art_data/";
    const std::string out_dir = root + "/build/sim/";

    Resources res;
    std::vector<std::vector<uint8_t>> keep;
    for (int i = 0; i < layout::kFaceCount; ++i) {
        std::vector<uint8_t> blob;
        if (!read_file(font_dir + kFontNames[i] + ".bin", blob)) {
            return 1;
        }
        keep.push_back(std::move(blob));
        if (!res.font[i].load(keep.back().data(), keep.back().size())) {
            std::fprintf(stderr, "bad font %s\n", kFontNames[i]);
            return 1;
        }
    }
    for (int i = 0; i < kArtCount; ++i) {
        std::vector<uint8_t> blob;
        if (!read_file(art_dir + kArtNames[i] + ".bin", blob)) {
            return 1;
        }
        keep.push_back(std::move(blob));
        if (!res.art[i].load(keep.back().data(), keep.back().size())) {
            std::fprintf(stderr, "bad art %s\n", kArtNames[i]);
            return 1;
        }
    }

    std::vector<AppState> states;
    std::map<std::string, int> seen;
    std::vector<std::vector<std::pair<std::string, int>>> taps;  // state -> (region, to)
    states.push_back(AppState{});
    seen[key(states[0])] = 0;
    taps.push_back({});

    FrameBuffer fb;
    for (size_t i = 0; i < states.size() && i < kMaxRendered; ++i) {
        HitRegion regions[pages::kMaxRegions];
        const int n = pages::regions(states[i], regions, pages::kMaxRegions);
        for (int r = 0; r < n; ++r) {
            AppState next = states[i];
            if (!pages::on_tap(next, regions[r].id)) {
                continue;
            }
            const std::string k = key(next);
            int to;
            auto it = seen.find(k);
            if (it == seen.end()) {
                to = static_cast<int>(states.size());
                seen[k] = to;
                states.push_back(next);
                taps.push_back({});
            } else {
                to = it->second;
            }
            taps[i].emplace_back(regions[r].id, to);
        }
    }

    const size_t rendered = states.size() < kMaxRendered ? states.size() : kMaxRendered;
    std::string json = "[\n";
    for (size_t i = 0; i < rendered; ++i) {
        pages::render(fb, states[i], res);
        char path[256];
        std::snprintf(path, sizeof(path), "%sstate_%03zu.raw", out_dir.c_str(), i);
        FILE *f = std::fopen(path, "wb");
        if (f == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 1;
        }
        std::fwrite(fb.data(), 1, FrameBuffer::kBytes, f);
        std::fclose(f);

        HitRegion regions[pages::kMaxRegions];
        const int n = pages::regions(states[i], regions, pages::kMaxRegions);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
                      "{\"i\":%zu,\"page\":\"%s\",\"mood\":%d,\"energy\":%d,\"intention\":%d,"
                      "\"regions\":[",
                      i, pages::name(states[i].page), states[i].mood, states[i].energy,
                      states[i].intention);
        json += buf;
        for (int r = 0; r < n; ++r) {
            std::snprintf(buf, sizeof(buf), "%s{\"id\":\"%s\",\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
                          r ? "," : "", regions[r].id, regions[r].x, regions[r].y, regions[r].w,
                          regions[r].h);
            json += buf;
        }
        json += "],\"taps\":[";
        bool first_tap = true;
        for (size_t t = 0; t < taps[i].size(); ++t) {
            if (taps[i][t].second >= static_cast<int>(rendered)) {
                continue;
            }
            std::snprintf(buf, sizeof(buf), "%s{\"id\":\"%s\",\"to\":%d}", first_tap ? "" : ",",
                          taps[i][t].first.c_str(), taps[i][t].second);
            first_tap = false;
            json += buf;
        }
        json += "]},\n";
    }
    json += "]\n";
    FILE *f = std::fopen((out_dir + "states.json").c_str(), "wb");
    if (f == nullptr) {
        return 1;
    }
    std::fwrite(json.data(), 1, json.size(), f);
    std::fclose(f);

    std::printf("%zu reachable states -> %s\n", states.size(), out_dir.c_str());
    return 0;
}
