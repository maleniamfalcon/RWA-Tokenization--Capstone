//
//  GuiTabs.cpp
//  RWA GUI
//
//  HOW TO READ THIS FILE
//  - Every draw...() function runs about 60 times a second. It redraws its part
//    of the window from scratch each time.
//  - ImGui::Button(...) draws a button AND returns true on the frame it was
//    clicked, so the code inside   if (ImGui::Button(...)) { ... }   runs once per click.
//  - Anything the user types or picks is kept in a 'static' variable, so it
//    survives from one frame to the next.
//  - The engine (registry, ledger, coordinator, audit) is NOT created here.
//    main.cpp creates it once, before the loop, and passes it in by reference.
//
#include "GuiTabs.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Palette: Midnight Blue (surfaces), Royal Blue (primary), Burgundy (accent).
// Each value is (red, green, blue, alpha), 0..1.  Hex shown for reference.
// ---------------------------------------------------------------------------
const ImVec4 MIDNIGHT       (0.027f, 0.043f, 0.141f, 1.0f);   // #070B24  page background
const ImVec4 MIDNIGHT_DEEP  (0.016f, 0.027f, 0.098f, 1.0f);   // #040719  nav bar
const ImVec4 MIDNIGHT_CARD  (0.059f, 0.086f, 0.216f, 1.0f);   // #0F1637  cards
const ImVec4 MIDNIGHT_INPUT (0.086f, 0.122f, 0.286f, 1.0f);   // #161F49  text boxes, combos
const ImVec4 MIDNIGHT_LINE  (0.125f, 0.169f, 0.365f, 1.0f);   // #202B5D  borders, separators

const ImVec4 ROYAL          (0.255f, 0.412f, 0.882f, 1.0f);   // #4169E1  Royal Blue
const ImVec4 ROYAL_HOVER    (0.353f, 0.498f, 0.941f, 1.0f);   // #5A7FF0
const ImVec4 ROYAL_PRESSED  (0.204f, 0.341f, 0.784f, 1.0f);   // #3457C8
const ImVec4 ROYAL_LIGHT    (0.537f, 0.647f, 1.000f, 1.0f);   // #89A5FF  links / eyebrow text

const ImVec4 BURGUNDY       (0.502f, 0.000f, 0.125f, 1.0f);   // #800020  Burgundy
const ImVec4 BURGUNDY_HOVER (0.620f, 0.102f, 0.227f, 1.0f);   // #9E1A3A
const ImVec4 BURGUNDY_PRESS (0.400f, 0.000f, 0.102f, 1.0f);   // #66001A

const ImVec4 TEXT           (0.910f, 0.925f, 1.000f, 1.0f);   // #E8ECFF
const ImVec4 WHITE          (1.0f, 1.0f, 1.0f, 1.0f);

// Status colours (named as before so the tab code reads the same).
const ImVec4 GREEN(0.290f, 0.871f, 0.502f, 1.0f);   // #4ADE80  pass / settled
const ImVec4 RED  (0.898f, 0.404f, 0.502f, 1.0f);   // #E56780  fail / rejected (light burgundy, readable on dark)
const ImVec4 GREY (0.549f, 0.588f, 0.761f, 1.0f);   // #8C96C2  secondary text

// Fonts (null = fall back to ImGui's built-in font).
ImFont* g_fontRegular = nullptr;
ImFont* g_fontBold    = nullptr;

// Navigation pages, in nav-bar order.
const char* const PAGE_NAMES[] = {"Holders", "Mint", "Transfer", "Audit", "Reports"};
const int         PAGE_COUNT   = 5;

// ---------------------------------------------------------------------------
// Settings for the asset: 1427 Maple St
// ---------------------------------------------------------------------------
const uint64_t MAX_SUPPLY      = 1600;   // tokens
const uint64_t PRICE_CENTS     = 5000;   // $50 per token
const uint64_t CAP_TOKENS      = 400;    // 25% ownership cap
const long     SECONDS_PER_DAY = 86400;

// Shared table look: soft horizontal lines, striped rows, padding at the edges.
const ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                    ImGuiTableFlags_PadOuterX;

// ---------------------------------------------------------------------------
// AuditEntry field names — the ONLY place this file touches them.
// If your AuditEntry uses other names, change them here and nothing else.
// ---------------------------------------------------------------------------
const std::string& entryHolder(const AuditEntry& e) { return e.id; }
const std::string& entryEvent (const AuditEntry& e) { return e.event; }
bool               entryPassed(const AuditEntry& e) { return e.passed; }
const std::string& entryReason(const AuditEntry& e) { return e.reason; }
std::time_t        entryTime  (const AuditEntry& e) { return e.timestamp; }

// ---------------------------------------------------------------------------
// Counters the Transfer tab adds to, and the status strip and Reports read.
// ---------------------------------------------------------------------------
int g_settled  = 0;
int g_rejected = 0;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

// 175000 cents -> "$1,750.00"
std::string money(uint64_t cents) {
    std::string whole = std::to_string(cents / 100);
    for (int i = static_cast<int>(whole.size()) - 3; i > 0; i -= 3) whole.insert(i, ",");
    std::ostringstream out;
    out << "$" << whole << "." << std::setw(2) << std::setfill('0') << cents % 100;
    return out.str();
}

// 400 tokens -> "25.00%"   (basis points: integer maths only, no double)
std::string ownershipPercent(uint64_t balance) {
    uint64_t bps = balance * 10000 / MAX_SUPPLY;
    std::ostringstream out;
    out << bps / 100 << "." << std::setw(2) << std::setfill('0') << bps % 100 << "%";
    return out.str();
}

// Sum of every holder's balance. By the ledger's invariant this equals total minted.
uint64_t sumOfBalances(HolderRegistry& registry, Ledger& book) {
    uint64_t sum = 0;
    for (const auto& entry : registry.getHolders()) sum += book.getbalance(entry.first);
    return sum;
}

// ---------------------------------------------------------------------------
// Styling helpers (cards, labels, badges, buttons)
// ---------------------------------------------------------------------------

ImU32  col(const ImVec4& c)                 { return ImGui::GetColorU32(c); }
ImVec4 withAlpha(ImVec4 c, float alpha)     { c.w = alpha; return c; }

// Text in the bold font at a given size.
void boldText(const char* text, float size, const ImVec4& color = TEXT) {
    ImGui::PushFont(g_fontBold, size);
    ImGui::TextColored(color, "%s", text);
    ImGui::PopFont();
}

// Small grey caption shown above a form field (website-style labels on top).
void fieldLabel(const char* text) {
    ImGui::PushFont(nullptr, 13.0f);
    ImGui::TextColored(GREY, "%s", text);
    ImGui::PopFont();
}

// Section heading inside a card: bold title and a thin line under it.
void heading(const char* text) {
    ImGui::Dummy(ImVec2(0, 4));
    boldText(text, 17.0f);
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 2));
}

// Height for a card that fills the rest of the page, but never gets squashed too small.
float fillHeight() { return std::max(ImGui::GetContentRegionAvail().y, 460.0f); }

// A rounded Midnight panel. Always pair with endCard().
void beginCard(const char* id, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, MIDNIGHT_CARD);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
void endCard() { ImGui::EndChild(); }

// Pill-shaped coloured label, e.g. PASS / FAIL, Verified / Pending.
void badge(const char* text, const ImVec4& color) {
    const ImVec2 pad(9, 2);
    ImGui::PushFont(g_fontBold, 12.0f);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const ImVec2 sz(ts.x + pad.x * 2, ts.y + pad.y * 2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), col(withAlpha(color, 0.16f)), sz.y * 0.5f);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.x + pad.x, p.y + pad.y), col(color), text);
    ImGui::Dummy(sz);
    ImGui::PopFont();
}

// Big full-width primary (Royal Blue) call-to-action button.
bool primaryButton(const char* label, float width = -FLT_MIN) {
    ImGui::PushFont(g_fontBold, 15.0f);
    bool clicked = ImGui::Button(label, ImVec2(width, 42));
    ImGui::PopFont();
    return clicked;
}

// Burgundy secondary button.
bool secondaryButton(const char* label, ImVec2 size = ImVec2(0, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button,        BURGUNDY);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, BURGUNDY_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  BURGUNDY_PRESS);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

// Grey header row for tables.
void tableHeaders() {
    ImGui::PushStyleColor(ImGuiCol_Text, GREY);
    ImGui::TableHeadersRow();
    ImGui::PopStyleColor();
}

// "Key ............ value" line, value right-aligned to the card edge.
void keyValue(const char* key, const std::string& value, const ImVec4& valueColor = TEXT) {
    const float startX = ImGui::GetCursorPosX();
    const float avail  = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(GREY, "%s", key);
    ImGui::SameLine(startX + avail - ImGui::CalcTextSize(value.c_str()).x);
    ImGui::TextColored(valueColor, "%s", value.c_str());
}

// Thin rounded progress bar (Royal Blue on Midnight).
void slimBar(float fraction, const ImVec4& color = ROYAL, float height = 6) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + height), col(MIDNIGHT_INPUT), height * 0.5f);
    if (fraction > 0)
        dl->AddRectFilled(p, ImVec2(p.x + w * fraction, p.y + height), col(color), height * 0.5f);
    ImGui::Dummy(ImVec2(w, height));
}

// Wide coloured result banner, e.g. SETTLED  A0001 -> A0002 | 50 tokens
void statusBanner(bool ok, const std::string& text) {
    const ImVec4 c = ok ? GREEN : RED;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x, h = 56;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(withAlpha(ok ? GREEN : BURGUNDY, ok ? 0.10f : 0.35f)), 12);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), col(withAlpha(c, 0.5f)), 12);

    const char* word = ok ? "SETTLED" : "REJECTED";
    ImGui::PushFont(g_fontBold, 18.0f);
    const ImVec2 ws = ImGui::CalcTextSize(word);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(p.x + 20, p.y + (h - ws.y) * 0.5f), col(c), word);
    ImGui::PopFont();
    const ImVec2 ts = ImGui::CalcTextSize(text.c_str());
    dl->AddText(ImVec2(p.x + 20 + ws.x + 18, p.y + (h - ts.y) * 0.5f), col(TEXT), text.c_str());
    ImGui::Dummy(ImVec2(w, h));
}

// One of the key-number cards at the top of each page. progress < 0 = no bar.
void kpiCard(const char* id, const char* label, const std::string& value, const std::string& sub,
             const ImVec4& accent, float width, float progress = -1.0f) {
    beginCard(id, ImVec2(width, 112));

    // Coloured strip along the top edge of the card.
    const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
    ImGui::PushClipRect(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), false);
    ImGui::GetWindowDrawList()->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + 3), col(accent),
                                              ImGui::GetStyle().ChildRounding, ImDrawFlags_RoundCornersTop);
    ImGui::PopClipRect();

    fieldLabel(label);
    boldText(value.c_str(), 24.0f);
    if (progress >= 0) { ImGui::Dummy(ImVec2(0, 1)); slimBar(progress, accent, 5); }
    else {
        ImGui::PushFont(nullptr, 13.0f);
        ImGui::TextColored(GREY, "%s", sub.c_str());
        ImGui::PopFont();
    }
    endCard();
}

// ---------------------------------------------------------------------------
// Form helpers
// ---------------------------------------------------------------------------

// Fills two lists from the registry: the IDs, and labels like "A0001 - Aisha Rahman (350)".
void holderLists(HolderRegistry& registry, Ledger& book,
                 std::vector<std::string>& ids, std::vector<std::string>& labels) {
    ids.clear();
    labels.clear();
    for (const auto& entry : registry.getHolders()) {
        ids.push_back(entry.first);
        labels.push_back(entry.first + " - " + entry.second.name +
                         "  (" + std::to_string(book.getbalance(entry.first)) + ")");
    }
}

// A dropdown of holders. 'selected' is the position in the list (0 = first).
void holderCombo(const char* label, const std::vector<std::string>& labels, int& selected) {
    fieldLabel(label);
    if (labels.empty()) {
        ImGui::TextColored(GREY, "No holders yet (register some on the Holders page)");
        return;
    }
    if (selected >= static_cast<int>(labels.size())) selected = 0;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo((std::string("##") + label).c_str(), labels[selected].c_str())) {
        for (int i = 0; i < static_cast<int>(labels.size()); i++) {
            bool isSelected = (i == selected);
            if (ImGui::Selectable(labels[i].c_str(), isSelected)) selected = i;
            if (isSelected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

// Position of an ID in the list, or -1.
int indexOf(const std::vector<std::string>& ids, const std::string& id) {
    for (int i = 0; i < static_cast<int>(ids.size()); i++)
        if (ids[i] == id) return i;
    return -1;
}

// Case-insensitive "does text contain filter?" used by the country search box.
bool containsIgnoreCase(const std::string& text, const char* filter) {
    std::string a = text, b = filter;
    for (char& c : a) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : b) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return a.find(b) != std::string::npos;
}

// Searchable dropdown over the jurisdiction table. Writes the picked row into countryIndex.
void drawCountryPicker(const std::vector<JurisdictionInfo>& countries, int& countryIndex) {
    static char search[64] = "";

    fieldLabel("Country");
    if (countries.empty()) {
        ImGui::TextColored(RED, "No countries loaded - check the database.");
        return;
    }
    if (countryIndex < 0 || countryIndex >= static_cast<int>(countries.size())) countryIndex = 0;

    const JurisdictionInfo& picked = countries[countryIndex];
    std::string preview = picked.name + " (" + picked.code + ")";

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##country", preview.c_str())) {
        // Focus the search box when the dropdown first opens so you can type straight away.
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
            search[0] = '\0';
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", "Search name or code...", search, sizeof(search));

        for (int i = 0; i < static_cast<int>(countries.size()); ++i) {
            const JurisdictionInfo& j = countries[i];
            if (search[0] != '\0' && !containsIgnoreCase(j.name, search) && !containsIgnoreCase(j.code, search))
                continue;

            std::string label = j.name + " (" + j.code + ")";
            if (!j.permitted) label += "  [restricted]";

            // Grey out restricted countries; they can still be picked (the rule will block transfers).
            if (!j.permitted) ImGui::PushStyleColor(ImGuiCol_Text, GREY);
            bool selected = (i == countryIndex);
            if (ImGui::Selectable(label.c_str(), selected)) countryIndex = i;
            if (selected) ImGui::SetItemDefaultFocus();
            if (!j.permitted) ImGui::PopStyleColor();
        }
        ImGui::EndCombo();
    }
}

// The 12 simulation investors, with their purchases. Used by "Load sample investors".
struct SampleInvestor {
    const char* name;
    bool        kyc;
    const char* country;
    int         lockupDays;
    uint64_t    dollars;
};
const SampleInvestor SAMPLE[] = {
    {"Aisha Rahman",  true,  "US",  0, 17500},
    {"Omar Siddiq",   true,  "GB",  0,  5000},
    {"Yusuf Karim",   true,  "US", 30, 15000},
    {"Fatima Haddad", true,  "AE",  0, 10000},
    {"James Carter",  true,  "US",  0,  7520},
    {"Priya Nair",    true,  "GB",  0,  4000},
    {"Noah Bennett",  true,  "US", 90,  6000},
    {"Layla Mansour", true,  "AE",  0,  2500},
    {"Lena Vogel",    true,  "DE",  0,     0},
    {"Wei Zhang",     true,  "CN",  0,     0},
    {"Sam Ortiz",     false, "US",  0,     0},
    {"Grace Okafor",  true,  "GB",  0, 10000},
};

} // namespace

// ===========================================================================
// THEME + FONTS
// ===========================================================================
void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);   // start from dark, then override everything visible

    // Shape: rounded, roomy, no harsh borders.
    s.WindowRounding    = 0;
    s.ChildRounding     = 14;
    s.FrameRounding     = 8;
    s.PopupRounding     = 10;
    s.GrabRounding      = 8;
    s.ScrollbarRounding = 8;
    s.WindowBorderSize  = 0;
    s.ChildBorderSize   = 1;
    s.PopupBorderSize   = 1;
    s.FrameBorderSize   = 0;
    s.WindowPadding     = ImVec2(16, 16);
    s.FramePadding      = ImVec2(12, 8);
    s.ItemSpacing       = ImVec2(12, 10);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.CellPadding       = ImVec2(10, 8);
    s.ScrollbarSize     = 12;
    s.SeparatorTextBorderSize = 1;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                 = TEXT;
    c[ImGuiCol_TextDisabled]         = GREY;
    c[ImGuiCol_WindowBg]             = MIDNIGHT;
    c[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = MIDNIGHT_CARD;
    c[ImGuiCol_Border]               = MIDNIGHT_LINE;
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]              = MIDNIGHT_INPUT;
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.110f, 0.157f, 0.365f, 1.0f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.133f, 0.188f, 0.427f, 1.0f);
    c[ImGuiCol_TitleBg]              = MIDNIGHT_DEEP;
    c[ImGuiCol_TitleBgActive]        = MIDNIGHT_DEEP;
    c[ImGuiCol_TitleBgCollapsed]     = MIDNIGHT_DEEP;
    c[ImGuiCol_MenuBarBg]            = MIDNIGHT_DEEP;
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = MIDNIGHT_LINE;
    c[ImGuiCol_ScrollbarGrabHovered] = ROYAL_PRESSED;
    c[ImGuiCol_ScrollbarGrabActive]  = ROYAL;
    c[ImGuiCol_CheckMark]            = ROYAL_LIGHT;
    c[ImGuiCol_SliderGrab]           = ROYAL;
    c[ImGuiCol_SliderGrabActive]     = ROYAL_HOVER;
    c[ImGuiCol_Button]               = ROYAL;
    c[ImGuiCol_ButtonHovered]        = ROYAL_HOVER;
    c[ImGuiCol_ButtonActive]         = ROYAL_PRESSED;
    c[ImGuiCol_Header]               = withAlpha(ROYAL, 0.35f);   // selected row in dropdowns
    c[ImGuiCol_HeaderHovered]        = withAlpha(ROYAL, 0.50f);
    c[ImGuiCol_HeaderActive]         = withAlpha(ROYAL, 0.65f);
    c[ImGuiCol_Separator]            = MIDNIGHT_LINE;
    c[ImGuiCol_SeparatorHovered]     = ROYAL;
    c[ImGuiCol_SeparatorActive]      = ROYAL_HOVER;
    c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PlotHistogram]        = ROYAL;                     // progress bars
    c[ImGuiCol_PlotHistogramHovered] = ROYAL_HOVER;
    c[ImGuiCol_TableHeaderBg]        = MIDNIGHT_INPUT;
    c[ImGuiCol_TableBorderStrong]    = MIDNIGHT_LINE;
    c[ImGuiCol_TableBorderLight]     = withAlpha(MIDNIGHT_LINE, 0.7f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.025f);
    c[ImGuiCol_TextSelectedBg]       = withAlpha(ROYAL, 0.45f);
    c[ImGuiCol_ModalWindowDimBg]     = withAlpha(MIDNIGHT_DEEP, 0.7f);
}

void loadFonts() {
    // Avenir Next ships with macOS. Face 7 = Regular, face 2 = Demi Bold.
    // If the file is missing, ImGui's built-in font is used instead.
    const char* path = "/System/Library/Fonts/Avenir Next.ttc";
    FILE* f = std::fopen(path, "rb");
    if (!f) return;
    std::fclose(f);

    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.FontNo    = 7;
    g_fontRegular = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);   // first font added = default
    cfg.FontNo    = 2;
    g_fontBold    = io.Fonts->AddFontFromFileTTF(path, 16.0f, &cfg);
}

// ===========================================================================
// NAV BAR + PAGE HEADER
// ===========================================================================
void drawNavBar(int& page) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;
    const float  h = 68;

    // Bar background + Royal -> Burgundy gradient line along the bottom.
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(MIDNIGHT_DEEP));
    dl->AddRectFilledMultiColor(ImVec2(p.x, p.y + h - 2), ImVec2(p.x + w, p.y + h),
                                col(ROYAL), col(BURGUNDY), col(BURGUNDY), col(ROYAL));

    // Logo: Royal Blue tile with a Burgundy dot, and the "D" monogram.
    const float  tile = 34;
    const ImVec2 lp(p.x + 32, p.y + (h - tile) * 0.5f);
    dl->AddRectFilled(lp, ImVec2(lp.x + tile, lp.y + tile), col(ROYAL), 9);
    dl->AddCircleFilled(ImVec2(lp.x + tile - 7, lp.y + 7), 4.5f, col(BURGUNDY));
    ImGui::PushFont(g_fontBold, 20.0f);
    const ImVec2 ds = ImGui::CalcTextSize("D");
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(lp.x + (tile - ds.x) * 0.5f, lp.y + (tile - ds.y) * 0.5f), col(WHITE), "D");

    // Wordmark.
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(lp.x + tile + 12, p.y + 14), col(WHITE), "Draza");
    ImGui::PopFont();
    ImGui::PushFont(g_fontBold, 11.0f);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(lp.x + tile + 12, p.y + 38), col(GREY), "RWA COMPLIANCE");
    ImGui::PopFont();

    // Links.
    float x = p.x + 250;
    ImGui::PushFont(g_fontBold, 15.0f);
    for (int i = 0; i < PAGE_COUNT; i++) {
        const ImVec2 ts  = ImGui::CalcTextSize(PAGE_NAMES[i]);
        const float  pad = 16, bw = ts.x + pad * 2;

        ImGui::SetCursorScreenPos(ImVec2(x, p.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("nav", ImVec2(bw, h - 2))) page = i;
        ImGui::PopID();
        const bool hovered = ImGui::IsItemHovered();
        const bool active  = (page == i);

        if (hovered && !active)
            dl->AddRectFilled(ImVec2(x + 2, p.y + 16), ImVec2(x + bw - 2, p.y + h - 16), col(ImVec4(1, 1, 1, 0.05f)), 8);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x + pad, p.y + (h - ts.y) * 0.5f),
                    col(active || hovered ? WHITE : GREY), PAGE_NAMES[i]);
        if (active)
            dl->AddRectFilled(ImVec2(x + pad, p.y + h - 6), ImVec2(x + bw - pad, p.y + h - 2), col(ROYAL_LIGHT), 2);
        x += bw + 4;
    }
    ImGui::PopFont();

    // Right side: "Engine online" pill with a green dot.
    ImGui::PushFont(g_fontBold, 13.0f);
    const char*  live = "Engine online";
    const ImVec2 ls   = ImGui::CalcTextSize(live);
    const float  pw = ls.x + 40, ph = 30;
    const ImVec2 pp(p.x + w - 32 - pw, p.y + (h - ph) * 0.5f);
    dl->AddRectFilled(pp, ImVec2(pp.x + pw, pp.y + ph), col(MIDNIGHT_CARD), ph * 0.5f);
    dl->AddRect(pp, ImVec2(pp.x + pw, pp.y + ph), col(MIDNIGHT_LINE), ph * 0.5f);
    dl->AddCircleFilled(ImVec2(pp.x + 16, pp.y + ph * 0.5f), 4, col(GREEN));
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(pp.x + 28, pp.y + (ph - ls.y) * 0.5f), col(TEXT), live);
    ImGui::PopFont();

    // Reserve the bar's space so the page content starts below it.
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, h));
}

void drawPageHeader(int page) {
    static const char* const titles[PAGE_COUNT] = {
        "Investor registry", "Token issuance", "Transfers", "Audit trail", "Reports"};
    static const char* const subtitles[PAGE_COUNT] = {
        "Onboard investors and track who owns 1427 Maple St.",
        "Turn investment dollars into property tokens at $50 each.",
        "Every transfer runs KYC, jurisdiction, lock-up and ownership-cap checks.",
        "Append-only record of every compliance decision.",
        "Cap table, supply and compliance outcomes at a glance."};
    if (page < 0 || page >= PAGE_COUNT) page = 0;

    boldText("TOKENIZED REAL ESTATE", 12.0f, ROYAL_LIGHT);
    boldText(titles[page], 32.0f, WHITE);
    ImGui::TextColored(GREY, "%s", subtitles[page]);
    ImGui::Dummy(ImVec2(0, 10));
}

// ===========================================================================
// STATUS STRIP — row of key-number cards on every page
// ===========================================================================
void drawStatusStrip(HolderRegistry& registry, Ledger& book, Audit& audit) {
    const uint64_t minted = sumOfBalances(registry, book);
    const float gap   = 16;
    const float width = (ImGui::GetContentRegionAvail().x - gap * 5) / 6;

    kpiCard("kpi_asset", "Asset", "$80,000", "1427 Maple St, Tulsa OK", ROYAL, width);
    ImGui::SameLine(0, gap);
    kpiCard("kpi_price", "Token price", "$50", "per token", BURGUNDY, width);
    ImGui::SameLine(0, gap);
    kpiCard("kpi_minted", "Minted",
            std::to_string(minted) + " / " + std::to_string(MAX_SUPPLY), "",
            ROYAL, width, static_cast<float>(minted) / MAX_SUPPLY);
    ImGui::SameLine(0, gap);
    kpiCard("kpi_holders", "Holders", std::to_string(registry.getHolders().size()), "registered investors",
            ROYAL_LIGHT, width);
    ImGui::SameLine(0, gap);
    kpiCard("kpi_transfers", "Transfers settled", std::to_string(g_settled),
            std::to_string(g_rejected) + " rejected", GREEN, width);
    ImGui::SameLine(0, gap);
    kpiCard("kpi_audit", "Audit entries", std::to_string(audit.getEntries().size()), "append-only",
            BURGUNDY, width);

    ImGui::Dummy(ImVec2(0, 8));
}

// ===========================================================================
// TAB 1 — HOLDERS
// ===========================================================================
void drawHoldersTab(HolderRegistry& registry, Ledger& book,
                    const std::vector<JurisdictionInfo>& countries) {
    // ---- form values (static = kept between frames) ----
    static char name[64]     = "";
    static int  countryIndex = 0;
    static bool kyc          = true;
    static int  lockDays     = 0;
    static std::string status;
    static bool sampleLoaded = false;

    const float height = fillHeight();

    // Left card: the form.
    beginCard("holders_form", ImVec2(380, height));
    heading("Register a holder");
    fieldLabel("Full name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##name", "e.g. Aisha Rahman", name, sizeof(name));
    drawCountryPicker(countries, countryIndex);
    fieldLabel("Lock-up days");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputInt("##lockdays", &lockDays);
    if (lockDays < 0) lockDays = 0;
    ImGui::Checkbox("KYC verified", &kyc);
    ImGui::Dummy(ImVec2(0, 4));

    if (primaryButton("Register holder")) {
        if (name[0] == '\0') {
            status = "Enter a name first.";
        } else if (countries.empty()) {
            status = "No countries loaded - cannot register.";
        } else {
            Holder h;                                  // id left empty -> the registry makes one
            h.name             = name;
            h.KYCVerified      = kyc;
            h.jurisdictionCode = countries[countryIndex].code;
            h.lockupEndDate    = lockDays > 0 ? std::time(nullptr) + lockDays * SECONDS_PER_DAY : 0;

            std::string id = registry.addHolder(h);   // ENGINE: create the holder
            if (id.empty()) {
                status = "Registration failed.";
            } else {
                book.openAccount(id);                  // ENGINE: give them a 0-token account
                status = "Registered " + id + " (" + std::string(name) + ")";
                name[0] = '\0';                        // clear the box for the next person
            }
        }
    }
    if (!status.empty()) ImGui::TextColored(ROYAL_LIGHT, "%s", status.c_str());

    ImGui::Dummy(ImVec2(0, 8));
    heading("Sample data");
    if (sampleLoaded) ImGui::BeginDisabled();        // grey the button out after one use
    if (secondaryButton("Load sample investors", ImVec2(-FLT_MIN, 38))) {
        const std::time_t now = std::time(nullptr);
        for (const SampleInvestor& s : SAMPLE) {
            Holder h;
            h.name             = s.name;
            h.KYCVerified      = s.kyc;
            h.jurisdictionCode = s.country;
            h.lockupEndDate    = s.lockupDays > 0 ? now + s.lockupDays * SECONDS_PER_DAY : 0;
            std::string id = registry.addHolder(h);
            if (id.empty()) continue;
            book.openAccount(id);
            if (s.dollars > 0) book.mint(id, s.dollars * 100 / PRICE_CENTS);   // dollars -> tokens
        }
        sampleLoaded = true;
        status = "Loaded 12 sample investors.";
    }
    if (sampleLoaded) ImGui::EndDisabled();
    ImGui::TextColored(GREY, "Use once, at the start of a demo.");
    endCard();

    // Right card: the table.
    ImGui::SameLine(0, 20);
    beginCard("holders_table", ImVec2(0, height));
    heading(("Holders (" + std::to_string(registry.getHolders().size()) + ")").c_str());

    if (ImGui::BeginTable("holders", 7, TABLE_FLAGS | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupScrollFreeze(0, 1);           // header stays visible while scrolling
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Country");
        ImGui::TableSetupColumn("KYC");
        ImGui::TableSetupColumn("Lock-up");
        ImGui::TableSetupColumn("Balance");
        ImGui::TableSetupColumn("Ownership");
        tableHeaders();

        const std::time_t now = std::time(nullptr);
        for (const auto& entry : registry.getHolders()) {   // entry.first = ID, entry.second = Holder
            const std::string& id = entry.first;
            const Holder& h       = entry.second;
            uint64_t balance      = book.getbalance(id);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(ROYAL_LIGHT, "%s", id.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(h.name.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(h.jurisdictionCode.c_str());
            ImGui::TableNextColumn(); badge(h.KYCVerified ? "Verified" : "Pending", h.KYCVerified ? GREEN : RED);
            ImGui::TableNextColumn();
            if (h.lockupEndDate > now) ImGui::Text("%ld days", static_cast<long>((h.lockupEndDate - now) / SECONDS_PER_DAY) + 1);
            else                       ImGui::TextColored(GREY, "-");
            ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(balance));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(ownershipPercent(balance).c_str());
        }
        ImGui::EndTable();
    }
    endCard();
}

// ===========================================================================
// TAB 2 — MINT
// ===========================================================================
void drawMintTab(HolderRegistry& registry, Ledger& book) {
    static int selected = 0;
    static int dollars  = 0;
    static std::string status;
    struct Issuance { std::string id; uint64_t cents; uint64_t tokens; bool ok; };
    static std::vector<Issuance> issued;               // GUI's own list (the ledger's history is private)

    std::vector<std::string> ids, labels;
    holderLists(registry, book, ids, labels);

    uint64_t minted    = sumOfBalances(registry, book);
    uint64_t remaining = MAX_SUPPLY - minted;
    const float height = fillHeight();

    beginCard("mint_form", ImVec2(400, height));
    heading("Issue tokens");
    holderCombo("Holder", labels, selected);
    fieldLabel("Investment ($)");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputInt("##dollars", &dollars, 50, 1000);
    if (dollars < 0) dollars = 0;

    // Live preview: recalculated every frame as the user types.
    uint64_t cents  = static_cast<uint64_t>(dollars) * 100;
    uint64_t tokens = cents / PRICE_CENTS;             // whole tokens
    uint64_t refund = cents % PRICE_CENTS;             // leftover cents
    ImGui::Dummy(ImVec2(0, 2));
    keyValue("Tokens", std::to_string(tokens), WHITE);
    keyValue("Refund", money(refund));
    keyValue("Supply after", std::to_string(minted + tokens) + " / " + std::to_string(MAX_SUPPLY));
    ImGui::Dummy(ImVec2(0, 4));

    if (primaryButton("Mint tokens") && !ids.empty()) {
        const std::string& id = ids[selected];
        bool ok = tokens > 0 && book.mint(id, tokens);  // ENGINE: refuses 0 or past the supply cap
        issued.push_back({id, cents, tokens, ok});
        if (ok)               status = "Minted " + std::to_string(tokens) + " tokens to " + id;
        else if (tokens == 0) status = "Enter at least $50 (one token).";
        else                  status = "Refused: only " + std::to_string(remaining) + " tokens left.";
    }
    if (!status.empty()) ImGui::TextColored(ROYAL_LIGHT, "%s", status.c_str());

    ImGui::Dummy(ImVec2(0, 8));
    heading("Supply");
    float fraction = static_cast<float>(minted) / static_cast<float>(MAX_SUPPLY);
    std::string overlay = std::to_string(minted) + " / " + std::to_string(MAX_SUPPLY);
    ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0), overlay.c_str());   // -FLT_MIN = full width
    ImGui::TextColored(GREY, "%llu tokens left | %s", static_cast<unsigned long long>(remaining),
                       money(remaining * PRICE_CENTS).c_str());
    endCard();

    ImGui::SameLine(0, 20);
    beginCard("mint_table", ImVec2(0, height));
    heading("Recent issuances (this session)");
    if (ImGui::BeginTable("issued", 5, TABLE_FLAGS | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Holder");
        ImGui::TableSetupColumn("Paid");
        ImGui::TableSetupColumn("Tokens");
        ImGui::TableSetupColumn("Refund");
        ImGui::TableSetupColumn("Result");
        tableHeaders();
        for (int i = static_cast<int>(issued.size()) - 1; i >= 0; i--) {   // newest first
            const Issuance& r = issued[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(ROYAL_LIGHT, "%s", r.id.c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(money(r.cents).c_str());
            ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(r.tokens));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(money(r.cents % PRICE_CENTS).c_str());
            ImGui::TableNextColumn(); badge(r.ok ? "MINTED" : "REFUSED", r.ok ? GREEN : RED);
        }
        ImGui::EndTable();
    }
    if (issued.empty()) ImGui::TextColored(GREY, "Nothing minted yet this session.");
    endCard();
}

// ===========================================================================
// TAB 3 — TRANSFER
// ===========================================================================
void drawTransferTab(HolderRegistry& registry, Ledger& book, TransferCoordinator& coord, Audit& audit) {
    static int fromIndex = 0;
    static int toIndex   = 1;
    static int amount    = 0;
    static std::string message;

    // Result of the last submit, kept so it stays on screen.
    static bool hasResult = false;
    static bool lastOk    = false;
    static std::string lastSummary;
    static std::vector<AuditEntry> lastChecks;
    static uint64_t fromBefore = 0, toBefore = 0, fromAfter = 0, toAfter = 0;
    static std::string lastFrom, lastTo;

    std::vector<std::string> ids, labels;
    holderLists(registry, book, ids, labels);
    const float height = fillHeight();

    beginCard("transfer_form", ImVec2(420, height));
    heading("New transfer");
    holderCombo("From", labels, fromIndex);
    holderCombo("To", labels, toIndex);
    fieldLabel("Tokens");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputInt("##tokens", &amount);
    if (amount < 0) amount = 0;
    ImGui::Dummy(ImVec2(0, 4));

    if (primaryButton("Submit transfer") && !ids.empty()) {
        lastFrom   = ids[fromIndex];
        lastTo     = ids[toIndex];
        fromBefore = book.getbalance(lastFrom);
        toBefore   = book.getbalance(lastTo);

        size_t before = audit.getEntries().size();                         // where this transfer's audit lines start
        lastOk = coord.submit(lastFrom, lastTo, static_cast<uint64_t>(amount));   // ENGINE: the whole pipeline
        lastOk ? g_settled++ : g_rejected++;

        lastChecks.clear();                                                // copy just this transfer's lines
        const auto& entries = audit.getEntries();
        for (size_t i = before; i < entries.size(); i++) lastChecks.push_back(entries[i]);

        fromAfter   = book.getbalance(lastFrom);
        toAfter     = book.getbalance(lastTo);
        lastSummary = lastFrom + " -> " + lastTo + " | " + std::to_string(amount) + " tokens";
        hasResult   = true;
        message.clear();
    }

    ImGui::Dummy(ImVec2(0, 8));
    heading("Demo presets");
    ImGui::TextColored(GREY, "Fill the form with a known case (needs the sample investors).");
    struct Preset { const char* label; const char* from; const char* to; int tokens; };
    const Preset presets[] = {
        {"Settles",            "A0001", "A0002",  50},
        {"KYC fail",           "A0001", "A0011",  10},
        {"Jurisdiction fail",  "A0004", "A0009",  20},
        {"Lock-up fail",       "A0003", "A0005",  50},
        {"Ownership cap fail", "A0012", "A0001", 120},
        {"Low balance",        "A0008", "A0002", 500},
    };
    const float presetWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    for (int i = 0; i < 6; i++) {
        if (i % 2 == 1) ImGui::SameLine();                                 // two buttons per row
        // The one preset that succeeds is Royal Blue; the failure cases are Burgundy.
        bool clicked = (i == 0) ? ImGui::Button(presets[i].label, ImVec2(presetWidth, 0))
                                : secondaryButton(presets[i].label, ImVec2(presetWidth, 0));
        if (clicked) {
            int f = indexOf(ids, presets[i].from), t = indexOf(ids, presets[i].to);
            if (f < 0 || t < 0) {
                message = "Load the sample investors first (Holders page).";
            } else {
                fromIndex = f; toIndex = t; amount = presets[i].tokens;
                message = std::string("Preset: ") + presets[i].label + ". Now click Submit transfer.";
            }
        }
    }
    if (!message.empty()) ImGui::TextColored(ROYAL_LIGHT, "%s", message.c_str());
    endCard();

    ImGui::SameLine(0, 20);
    beginCard("transfer_result", ImVec2(0, height));
    heading("Result");
    if (!hasResult) {
        ImGui::TextColored(GREY, "Submit a transfer to see every compliance check here.");
    } else {
        statusBanner(lastOk, lastSummary);
        ImGui::Dummy(ImVec2(0, 4));

        if (ImGui::BeginTable("checks", 3, TABLE_FLAGS)) {
            ImGui::TableSetupColumn("Check", ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Reason");
            tableHeaders();
            for (const AuditEntry& e : lastChecks) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(entryEvent(e).c_str());
                ImGui::TableNextColumn(); badge(entryPassed(e) ? "PASS" : "FAIL", entryPassed(e) ? GREEN : RED);
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s", entryReason(e).c_str());
            }
            ImGui::EndTable();
        }

        heading("Balances");
        keyValue(lastFrom.c_str(), std::to_string(fromBefore) + "  ->  " + std::to_string(fromAfter));
        keyValue(lastTo.c_str(),   std::to_string(toBefore)   + "  ->  " + std::to_string(toAfter));
    }
    endCard();
}

// ===========================================================================
// TAB 4 — AUDIT
// ===========================================================================
void drawAuditTab(Audit& audit) {
    static char holderFilter[16] = "";
    static bool failuresOnly     = false;

    beginCard("audit_card", ImVec2(0, fillHeight()));

    // Filter row.
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##holderfilter", "Filter by holder ID", holderFilter, sizeof(holderFilter));
    ImGui::SameLine(0, 20);
    ImGui::Checkbox("Failures only", &failuresOnly);
    ImGui::SameLine(0, 20);
    if (secondaryButton("Clear filter")) { holderFilter[0] = '\0'; failuresOnly = false; }
    ImGui::Dummy(ImVec2(0, 4));

    const auto& entries = audit.getEntries();
    int shown = 0;

    if (ImGui::BeginTable("audit", 5, TABLE_FLAGS | ImGuiTableFlags_ScrollY,
                          ImVec2(0, -ImGui::GetTextLineHeightWithSpacing() * 1.5f))) {   // leave room for the count
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Time",   ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Holder", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Event",  ImGuiTableColumnFlags_WidthFixed, 190);
        ImGui::TableSetupColumn("Result", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Reason");
        tableHeaders();

        for (const AuditEntry& e : entries) {
            if (failuresOnly && entryPassed(e)) continue;                              // filter 1
            if (holderFilter[0] != '\0' && entryHolder(e) != holderFilter) continue;   // filter 2
            shown++;

            std::time_t t = entryTime(e);
            std::ostringstream time;
            time << std::put_time(std::localtime(&t), "%H:%M:%S");

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(GREY, "%s", time.str().c_str());
            ImGui::TableNextColumn(); ImGui::TextColored(ROYAL_LIGHT, "%s", entryHolder(e).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(entryEvent(e).c_str());
            ImGui::TableNextColumn(); badge(entryPassed(e) ? "PASS" : "FAIL", entryPassed(e) ? GREEN : RED);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(entryReason(e).c_str());
        }
        ImGui::EndTable();
    }
    ImGui::TextColored(GREY, "Showing %d of %zu entries | append-only", shown, entries.size());
    endCard();
}

// ===========================================================================
// TAB 5 — REPORTS   (no buttons: everything is recalculated every frame)
// ===========================================================================
void drawReportsTab(HolderRegistry& registry, Ledger& book, Audit& audit) {
    uint64_t minted = sumOfBalances(registry, book);
    const float height = fillHeight();

    beginCard("cap_table", ImVec2(ImGui::GetContentRegionAvail().x * 0.6f, height));
    heading("Cap table");
    if (ImGui::BeginTable("cap", 5, TABLE_FLAGS | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Holder");
        ImGui::TableSetupColumn("Tokens");
        ImGui::TableSetupColumn("Ownership");
        ImGui::TableSetupColumn("Toward 25% cap");
        ImGui::TableSetupColumn("Value");
        tableHeaders();
        for (const auto& entry : registry.getHolders()) {
            uint64_t balance = book.getbalance(entry.first);
            if (balance == 0) continue;                                    // hide empty accounts
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(ROYAL_LIGHT, "%s", entry.first.c_str());
                                      ImGui::SameLine(); ImGui::TextUnformatted(entry.second.name.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(balance));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(ownershipPercent(balance).c_str());
            ImGui::TableNextColumn();
            // Bar turns Burgundy once a holder is within 20% of the ownership cap.
            ImGui::Dummy(ImVec2(0, 4));
            slimBar(static_cast<float>(balance) / CAP_TOKENS, balance * 5 >= CAP_TOKENS * 4 ? RED : ROYAL, 8);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(money(balance * PRICE_CENTS).c_str());
        }
        ImGui::EndTable();
    }
    endCard();

    ImGui::SameLine(0, 20);
    beginCard("summary", ImVec2(0, height));
    heading("Supply");
    ImGui::ProgressBar(static_cast<float>(minted) / MAX_SUPPLY, ImVec2(-FLT_MIN, 0),
                       (std::to_string(minted) + " / " + std::to_string(MAX_SUPPLY)).c_str());
    keyValue("Value issued", money(minted * PRICE_CENTS) + " of $80,000.00");
    keyValue("Unsold", std::to_string(MAX_SUPPLY - minted) + " tokens");

    heading("Invariant");
    // The sum of balances is also how 'minted' is computed here, so this checks it
    // against the ledger's own counter when your Ledger has totalMinted():
    //     bool ok = (minted == book.totalMinted());
    badge("HOLDS", GREEN);
    ImGui::SameLine();
    ImGui::Text("Sum of balances = %llu tokens", static_cast<unsigned long long>(minted));

    heading("Transfers");
    keyValue("Settled",  std::to_string(g_settled),  GREEN);
    keyValue("Rejected", std::to_string(g_rejected), RED);

    std::map<std::string, int> failsByEvent;                               // e.g. "KYC Rule" -> 2
    for (const AuditEntry& e : audit.getEntries())
        if (!entryPassed(e)) failsByEvent[entryEvent(e)]++;

    ImGui::Dummy(ImVec2(0, 4));
    if (ImGui::BeginTable("fails", 2, TABLE_FLAGS)) {
        ImGui::TableSetupColumn("Failed check");
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 60);
        tableHeaders();
        for (const auto& f : failsByEvent) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(f.first.c_str());
            ImGui::TableNextColumn(); ImGui::TextColored(RED, "%d", f.second);
        }
        ImGui::EndTable();
    }
    endCard();
}
