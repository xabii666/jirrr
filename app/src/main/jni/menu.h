#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>
#include "include/font_arial_black.h"
#include "include/font_segoeui.h"
#include "include/font_fontawesome.h"

#include "icons/icons.h"

using namespace ImGui;
using namespace std;

#include <chrono>
#include <thread>
#include "include/includes.h"

#include "game.h"
#include "game/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "mod/keylogin.h"
#include "oxorany/oxorany.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <sys/system_properties.h>
#include <ctime>

#define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))

struct MenuState {
    bool isOpen = false;
    int currentTab = 0;
    float sidebarW = 860.0f; // Increased from 820.0f
    float animProgress = 0.0f;
    float menuAlpha = 0.0f;
    float menuScale = 0.9f;
    ImVec4 accentColor = ImVec4(0.35f, 0.65f, 0.95f, 1.0f);
};
static MenuState g_menu;

static ImFont* g_ArialBlackFont = nullptr;
static ImFont* g_SegoeUIFont = nullptr;
static ImFont* g_IconFont = nullptr;

// Modified global expiry to support dynamic bypass keys
static int64_t G_EXPIRY_TS = 2000000000LL; // Default placeholder

static string formatTimestamp(int64_t ts) {
    if (ts <= 0) return O("N/A");
    time_t t = (time_t)ts;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_info);
    return string(buffer);
}

static bool DEBUG_BYPASS_LOGIN = false; // Set to true to test menu without firebase login

static const int CLIENT_VERSION = 1; // Current version of this APK/library
static bool g_UpdateRequired = false;
static string g_UpdateURL = "";

static void CheckAppUpdate() {
    thread([]() {
        string url = "https://firestore.googleapis.com/v1/projects/" + FIREBASE_PROJECT_ID + "/databases/(default)/documents/settings/global?key=" + FIREBASE_API_KEY;
        string response = HttpRequest(url, "GET");
        if (!response.empty()) {
            try {
                auto respJson = json::parse(response);
                if (respJson.contains("fields")) {
                    auto fields = respJson["fields"];
                    
                    bool forceUpdateVal = false;
                    if (fields.contains("force_update") && fields["force_update"].contains("booleanValue")) {
                        forceUpdateVal = fields["force_update"]["booleanValue"].get<bool>();
                    }

                    int requiredVersion = 1;
                    if (fields.contains("required_version") && fields["required_version"].contains("integerValue")) {
                        requiredVersion = std::stoi(fields["required_version"]["integerValue"].get<string>());
                    }

                    if (fields.contains("download_url") && fields["download_url"].contains("stringValue")) {
                        g_UpdateURL = fields["download_url"]["stringValue"].get<string>();
                    }

                    // Only require update if force_update is ON and the client version is older than required
                    g_UpdateRequired = forceUpdateVal && (CLIENT_VERSION < requiredVersion);
                }
            } catch (...) {
                // Ignore parse/network errors
            }
        }
    }).detach();
}

static float EaseOutBack(float x) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col1, ImU32 col2, bool horizontal = true) {
    if (horizontal) {
        dl->AddRectFilledMultiColor(p1, p2, col1, col2, col2, col1);
    } else {
        dl->AddRectFilledMultiColor(p1, p2, col1, col1, col2, col2);
    }
}

static float AddDashedLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, float thickness, float dash_len = 20.0f, float gap_len = 15.0f, float phase_offset = 0.0f) {
    ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (dist < 1.0f) return phase_offset;
    dir.x /= dist; dir.y /= dist;

    float cycle = dash_len + gap_len;
    float curr = -fmodf(phase_offset, cycle); // Start at correct phase offset
    if (curr > 0.0f) curr -= cycle;           // Ensure we start before beginning
    while (curr < dist) {
        float dashStart = curr;
        float dashEnd   = curr + dash_len;
        // Clamp to segment bounds
        float drawStart = ImMax(dashStart, 0.0f);
        float drawEnd   = ImMin(dashEnd,   dist);
        if (drawStart < drawEnd) {
            dl->AddLine(
                ImVec2(p1.x + dir.x * drawStart, p1.y + dir.y * drawStart),
                ImVec2(p1.x + dir.x * drawEnd,   p1.y + dir.y * drawEnd),
                col, thickness);
        }
        curr += cycle;
    }
    // Return how far we traveled so next segment continues the phase
    return fmodf(phase_offset + dist, cycle);
}


static bool SidebarButton(const char* label, const char* iconStr, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float iconSize   = 65.0f; // Restored original size to keep header correct
    float vPad       = 18.0f; // Restored original padding
    float btnH       = vPad + iconSize + 4.0f + g.FontSize + vPad;

    ImVec2 pos  = window->DC.CursorPos;
    ImVec2 size = ImVec2(width, btnH);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    // Icon center position
    ImVec2 iconCenter = ImVec2(
        bb.Min.x + width * 0.5f,
        bb.Min.y + vPad + iconSize * 0.5f
    );

    // Selected background/border circles have been removed as requested

    // Draw icon text centered: Orange when selected/unselected, highlighting when active
    if (iconStr && g_IconFont) {
        PushFont(g_IconFont);
        ImVec2 iconTextSize = CalcTextSize(iconStr);
        ImVec2 iconPos = ImVec2(
            iconCenter.x - iconTextSize.x * 0.5f,
            iconCenter.y - iconTextSize.y * 0.5f
        );
        
        ImU32 tint = selected ? IM_COL32(255, 160, 0, 255) : IM_COL32(180, 90, 0, 180);
        
        if (selected) {
            // Text bloom shadow
            dl->AddText(iconPos - ImVec2(1.0f, 0.0f), IM_COL32(255, 120, 0, 80), iconStr);
            dl->AddText(iconPos + ImVec2(1.0f, 0.0f), IM_COL32(255, 120, 0, 80), iconStr);
            dl->AddText(iconPos - ImVec2(0.0f, 1.0f), IM_COL32(255, 120, 0, 80), iconStr);
            dl->AddText(iconPos + ImVec2(0.0f, 1.0f), IM_COL32(255, 120, 0, 80), iconStr);
        }
        
        dl->AddText(iconPos, tint, iconStr);
        PopFont();
    }

    // Draw label centered below icon
    ImVec2 labelSize = CalcTextSize(label);
    ImVec2 textPos   = ImVec2(
        bb.Min.x + (width - labelSize.x) * 0.5f,
        bb.Min.y + vPad + iconSize + 4.0f
    );
    ImU32 textCol = selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(140, 140, 150, 255);
    dl->AddText(textPos, textCol, label);

    return pressed;
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float scale = 2.0f; // Increased to 2.0f for a bigger switch
    float height = 34.0f * scale; // Slightly thicker
    float width = 56.0f * scale;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 pos = window->DC.CursorPos;
    // Increased vertical padding (28.0f -> 40.0f) for more height
    ImVec2 size = ImVec2(GetContentRegionAvail().x, ImMax(height, textSize.y) + style.FramePadding.y * 2 + 40.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    // Calculate toggle bounds early for interaction
    float sideMargin = 20.0f; 
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;
    float switchW = 76.0f * scale; // Slightly shorter width
    ImVec2 togglePos = ImVec2(bgRect.Max.x - switchW - 25.0f, bb.Min.y + (size.y - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + switchW, togglePos.y + height);
    
    // The interactive area is strictly around the toggle switch pill
    ImRect toggleBb(ImVec2(togglePos.x - 15.0f, togglePos.y - 15.0f), ImVec2(toggleEnd.x + 15.0f, toggleEnd.y + 15.0f));

    bool hovered, held;
    bool pressed = ButtonBehavior(toggleBb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> switchAnim;
    float& animT = switchAnim[id];
    float targetT = *v ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 14.0f;

    ImDrawList* dl = window->DrawList;
    
    // Permanent Box Background (Segmented style) using new smaller bgRect
    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(32, 32, 38, 255), 15.0f);
    
    if (hovered) {
        dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(50, 50, 60, 100), 15.0f); // Hover highlight
    }
    
    
    ImVec4 offColor = ImVec4(0.24f, 0.24f, 0.28f, 1.0f);
    ImVec4 onColor = ImVec4(1.0f, 0.47f, 0.0f, 1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, animT);
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);
    
    // --- ON/OFF Text Labels inside the track ---
    float innerTextScale = 0.85f;
    SetWindowFontScale(innerTextScale);
    
    if (*v) {
        // Center "ON" in the Left Half of the track (since knob is on the right)
        ImVec2 onSize = CalcTextSize("ON");
        ImVec2 onPos = ImVec2(togglePos.x + (switchW * 0.5f - onSize.x) * 0.5f + 5.0f, togglePos.y + (height - onSize.y) * 0.5f);
        dl->AddText(onPos, IM_COL32(255, 255, 255, 255), "ON");
    } else {
        // Center "OFF" in the Right Half of the track (since knob is on the left)
        ImVec2 offSize = CalcTextSize("OFF");
        ImVec2 offPos = ImVec2(togglePos.x + switchW * 0.5f + (switchW * 0.5f - offSize.x) * 0.5f - 5.0f, togglePos.y + (height - offSize.y) * 0.5f);
        dl->AddText(offPos, IM_COL32(220, 220, 230, 255), "OFF");
    }
    SetWindowFontScale(1.0f);
    
    float knobX = togglePos.x + radius + (switchW - height) * animT;
    float knobY = togglePos.y + radius;
    float knobR = radius - 6.0f; // Smaller knob, sits inside the track
    
    ImVec4 knobOffColor = ImVec4(0.05f, 0.05f, 0.05f, 1.0f); // almost black
    ImVec4 knobOnColor  = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);    // white
    ImVec4 knobColorV   = ImLerp(knobOffColor, knobOnColor, animT);
    
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, ImColor(knobColorV));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(0, 0, 0, 30)); // Subtle shadow

    // Align text further inside the bgRect (15.0f -> 25.0f)
    dl->AddText(ImVec2(bgRect.Min.x + 25.0f, bb.Min.y + (size.y - textSize.y) * 0.5f), IM_COL32(230, 230, 240, 255), label);

    return pressed;
}

static bool CustomSliderFloat(const char* label, float* v, float v_min, float v_max, const char* format = "%.2f") {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float sliderHeight = 45.0f; // Height of the slider bar itself
    ImVec2 labelSize = CalcTextSize(label);
    
    ImVec2 pos = window->DC.CursorPos;
    // Taller height for the slider itself without the extra box padding
    ImVec2 size = ImVec2(GetContentRegionAvail().x, labelSize.y + sliderHeight + style.FramePadding.y * 2 + 25.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    // Alignment Margins (Matching ToggleSwitch placement but without the box)
    float sideMargin = 20.0f; 
    float internalPadding = 25.0f;
    float totalSideOffset = sideMargin + internalPadding;
    float contentWidth = bb.GetWidth() - (totalSideOffset * 2.0f);

    // Calculate knob position early for interaction
    ImVec2 barPos = ImVec2(bb.Min.x + totalSideOffset, bb.Min.y + labelSize.y + 20.0f);
    float barRadius = sliderHeight * 0.5f;
    float grabPos = (*v - v_min) / (v_max - v_min) * contentWidth;
    ImVec2 knobPos = ImVec2(barPos.x + grabPos, barPos.y + barRadius);
    float hitRadius = ImMax(sliderHeight * 0.7f + 25.0f, 60.0f); // Large touch area strictly around the knob
    ImRect knobBb(ImVec2(knobPos.x - hitRadius, knobPos.y - hitRadius), ImVec2(knobPos.x + hitRadius, knobPos.y + hitRadius));

    bool hovered, held;
    bool pressed = ButtonBehavior(knobBb, id, &hovered, &held);

    if (held) {
        *v = CLAMP((g.IO.MousePos.x - (bb.Min.x + totalSideOffset)) / contentWidth * (v_max - v_min) + v_min, v_min, v_max);
    }

    ImDrawList* dl = window->DrawList;
    
    // Label (No background frame drawn here anymore)
    dl->AddText(ImVec2(bb.Min.x + totalSideOffset, bb.Min.y + 5.0f), IM_COL32(230, 230, 240, 255), label);
    
    // Value text
    char value_buf[32];
    if (strstr(label, "Percent") || strstr(label, "Transparency")) {
        snprintf(value_buf, sizeof(value_buf), "%d%%", (int)(*v * (strstr(label, "Transparency") ? 100.0f : 1.0f)));
    } else if (strstr(label, "Thickness") || strstr(label, "Size") || strstr(label, "Point")) {
        snprintf(value_buf, sizeof(value_buf), "%.1f", *v);
    } else {
        snprintf(value_buf, sizeof(value_buf), format, *v);
    }
    
    ImVec2 valSize = CalcTextSize(value_buf);
    dl->AddText(ImVec2(bb.Max.x - totalSideOffset - valSize.x, bb.Min.y + 5.0f), IM_COL32(255, 120, 0, 255), value_buf);

    // Slider Bar
    ImVec2 barEnd = ImVec2(barPos.x + contentWidth, barPos.y + sliderHeight);

    // Slider Background (This is the track, not the frame)
    dl->AddRectFilled(barPos, barEnd, IM_COL32(32, 32, 38, 255), barRadius);

    // Slider Fill
    if (grabPos > 0.0f) {
        dl->AddRectFilled(barPos, ImVec2(barPos.x + grabPos, barEnd.y), IM_COL32(255, 120, 0, 255), barRadius);
    }

    // Knob
    dl->AddCircleFilled(knobPos, 32.0f, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(knobPos, 32.0f, IM_COL32(0, 0, 0, 30)); 

    return held;
}

static bool CustomSliderInt(const char* label, int* v, int v_min, int v_max) {
    float f_val = (float)*v;
    if (CustomSliderFloat(label, &f_val, (float)v_min, (float)v_max, "%.0f")) {
        *v = (int)f_val;
        return true;
    }
    return false;
}

static bool SegmentedControl(const char* label, int* current, const std::vector<std::string>& options, float controlHeight = 110.0f) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    // Set size including vertical spacing
    ImVec2 size = ImVec2(GetContentRegionAvail().x, controlHeight + 20.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    // Frame Style (Matches ToggleSwitch: 20f Margin, 15f Radius)
    float sideMargin = 20.0f;
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(32, 32, 38, 255), 15.0f);

    // Internal Padding for the segments (25.0f matches Slider/Toggle)
    float internalPadding = 25.0f; 
    ImRect contentRect = bgRect;
    contentRect.Min += ImVec2(internalPadding, internalPadding);
    contentRect.Max -= ImVec2(internalPadding, internalPadding);

    float width = contentRect.Max.x - contentRect.Min.x;
    float height = contentRect.Max.y - contentRect.Min.y;
    int count = (int)options.size();
    float itemW = width / count;

    bool changed = false;
    for (int i = 0; i < count; i++) {
        ImVec2 itemMin = ImVec2(contentRect.Min.x + i * itemW, contentRect.Min.y);
        ImVec2 itemMax = ImVec2(itemMin.x + itemW, itemMin.y + height);
        ImRect itemBb(itemMin, itemMax);
        
        ImGuiID itemId = window->GetID((std::string(label) + std::to_string(i)).c_str());
        bool hovered, held;
        if (ButtonBehavior(itemBb, itemId, &hovered, &held)) {
            *current = i;
            changed = true;
        }

        if (*current == i) {
            dl->AddRectFilled(itemMin, itemMax, IM_COL32(255, 120, 0, 255), 12.0f);
        } else if (hovered) {
            dl->AddRectFilled(itemMin, itemMax, IM_COL32(50, 50, 60, 150), 12.0f);
        }

        ImVec2 ts = CalcTextSize(options[i].c_str());
        ImU32 textCol = (*current == i) ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 170, 255);
        dl->AddText(ImVec2(itemMin.x + (itemW - ts.x) * 0.5f, itemMin.y + (height - ts.y) * 0.5f), textCol, options[i].c_str());
    }

    return changed;
}

static bool GlassSegmentedControl(const char* label, int* current, const std::vector<std::string>& options, float controlHeight = 100.0f) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, controlHeight + 20.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    float sideMargin = 20.0f;
    ImRect bgRect = bb;
    bgRect.Min.x += sideMargin;
    bgRect.Max.x -= sideMargin;

    ImDrawList* dl = window->DrawList;
    
    // Glass Background Effect
    dl->AddRectFilled(bgRect.Min, bgRect.Max, IM_COL32(45, 45, 55, 100), 12.0f);
    dl->AddRect(bgRect.Min, bgRect.Max, IM_COL32(255, 255, 255, 40), 12.0f, 0, 1.5f);

    float internalPadding = 18.0f; 
    ImRect contentRect = bgRect;
    contentRect.Min += ImVec2(internalPadding, internalPadding);
    contentRect.Max -= ImVec2(internalPadding, internalPadding);

    float width = contentRect.Max.x - contentRect.Min.x;
    float height = contentRect.Max.y - contentRect.Min.y;
    int count = (int)options.size();
    float itemW = width / count;

    // Smooth Animation for Selector
    static std::map<ImGuiID, float> selectorAnim;
    float& animX = selectorAnim[id];
    float targetX = (float)(*current) * itemW;
    animX += (targetX - animX) * g.IO.DeltaTime * 15.0f;

    // Glowing Selector
    ImVec2 selMin = ImVec2(contentRect.Min.x + animX, contentRect.Min.y);
    ImVec2 selMax = ImVec2(selMin.x + itemW, contentRect.Max.y);
    dl->AddRectFilled(selMin, selMax, IM_COL32(255, 120, 0, 180), 10.0f);
    dl->AddRect(selMin, selMax, IM_COL32(255, 255, 255, 80), 10.0f, 0, 1.0f);

    bool changed = false;
    for (int i = 0; i < count; i++) {
        ImVec2 itemMin = ImVec2(contentRect.Min.x + i * itemW, contentRect.Min.y);
        ImVec2 itemMax = ImVec2(itemMin.x + itemW, itemMin.y + height);
        ImRect itemBb(itemMin, itemMax);
        
        ImGuiID itemId = window->GetID((std::string(label) + std::to_string(i)).c_str());
        bool hovered, held;
        if (ButtonBehavior(itemBb, itemId, &hovered, &held)) {
            *current = i;
            changed = true;
        }

        ImVec2 ts = CalcTextSize(options[i].c_str());
        ImU32 textCol = (*current == i) ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 160, 170, 255);
        dl->AddText(ImVec2(itemMin.x + (itemW - ts.x) * 0.5f, itemMin.y + (height - ts.y) * 0.5f), textCol, options[i].c_str());
    }

    return changed;
}

// File-scope so DrawToggleButton cancel can also reset countdown
static bool g_aqCounting = false;
static std::chrono::steady_clock::time_point g_aqLastCall;
static std::chrono::steady_clock::time_point g_aqCountdownStart;


static bool IsExpired() {
    return (int64_t)time(nullptr) >= G_EXPIRY_TS;
}

// Helper to update Android Window Flags for touch transparency
static void UpdateOverlayFlags(bool touchable) {
    JNIEnv* env = GetJNIEnv();
    if (!env) return;
    
    // This is a common JNI pattern to update flags on the fly
    // We target the current window and toggle FLAG_NOT_TOUCHABLE (0x10)
    static jclass viewClass = env->FindClass("android/view/View");
    static jclass paramsClass = env->FindClass("android/view/WindowManager$LayoutParams");
    
    // In a real implementation, we would need the specific View object of the overlay.
    // For now, we ensure ImGui's own internal touch handling is bypassed when closed.
    ImGuiIO& io = ImGui::GetIO();
    if (!touchable) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
}

INLINE void DrawExpired(ImGuiIO& io) {
    float winW = g_menu.sidebarW;

    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    SetNextWindowSize(ImVec2(winW, 0), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 30.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ExpiredWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize)) {

        SetWindowFontScale(1.6f);
        ImVec2 titleSz = CalcTextSize(O("MOD EXPIRED"));
        SetCursorPosX((winW - 60.0f - titleSz.x) * 0.5f);
        TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "%s", O("MOD EXPIRED"));
        SetWindowFontScale(1.0f);

        Dummy(ImVec2(0, 16));

        PushTextWrapPos(GetCursorPosX() + winW - 60.0f);
        TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s",
            O("Key Expired. Buy on our Telegram @Lionx_Engine"));
        PopTextWrapPos();

        Dummy(ImVec2(0, 10));
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}

INLINE void DrawAutoQueue() {
    if (((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0) || (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqLastCall).count() > 500)
            g_aqCounting = false;
        g_aqLastCall = now;

        if (!g_aqCounting) {
            g_aqCounting = true;
            g_aqCountdownStart = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_aqCountdownStart).count();
        int remaining_ms = 8000 - (int)elapsed;

        if (remaining_ms <= 0) {
            if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            StartLastMatch();
            g_aqCounting = false;
            return;
        }

        std::string count_str = std::to_string((remaining_ms / 1000) + 1);

        // Minimal auto-sized window, transparent bg — we draw our own rounded rect
        SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSizeConstraints(ImVec2(350.0f, -1.0f), ImVec2(450.0f, -1.0f)); // Fixed width range
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.92f));
        PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.47f, 0.0f, 0.8f)); // Dark Orange Border
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 20.0f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
        PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.5f);

        if (Begin(O("##AutoQueueCD"), nullptr,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_AlwaysAutoResize)) {
            
            ImDrawList* dl = GetWindowDrawList();
            ImVec2 ws = GetWindowSize();
            
            // 1. Stylish Header - Centered
            SetWindowFontScale(0.9f);
            float hTextWidth = CalcTextSize(O("AUTO QUEUE ACTIVE")).x;
            SetCursorPosX((ws.x - hTextWidth) * 0.5f);
            TextColored(ImVec4(0.7f, 0.7f, 0.75f, 1.0f), O("AUTO QUEUE ACTIVE"));
            Separator();
            Dummy(ImVec2(0, 5));

            // 2. Large Countdown Digit
            SetWindowFontScale(2.5f);
            ImVec2 ts = CalcTextSize(count_str.c_str());
            SetCursorPosX((ws.x - ts.x) * 0.5f);
            TextColored(ImVec4(1.0f, 0.47f, 0.0f, 1.0f), "%s", count_str.c_str());
            SetWindowFontScale(1.0f);
            
            Dummy(ImVec2(0, 8));

            // 3. Stylish Terminate Button
            float btnW = ws.x - 30.0f; // Increased width (closer to edges)
            float btnH = 55.0f; // Slightly taller for better touch
            SetCursorPosX(15.0f);
            
            PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.0f, 0.0f, 1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            SetWindowFontScale(0.9f);
            
            if (Button(O("TERMINATE"), ImVec2(btnW, btnH))) {
                persistent_bool[O("bAutoQueue")] = false;
                g_aqCounting = false;
            }
            
            SetWindowFontScale(1.0f);
            PopStyleVar();
            PopStyleColor(3);
        }
        End();
        PopStyleVar(3);
        PopStyleColor(2);
    }
}

#include "mod/ButtonClicker.h"

static ImU32 GetBallColor32(int i, float lineAlpha, bool isScanning) {
    static const ImU32 ballColors[] = {
        IM_COL32(255, 255, 255, 255), // 0: White
        IM_COL32(254, 228, 0,   255), // 1: Yellow
        IM_COL32(0,   89,  179, 255), // 2: Blue
        IM_COL32(216, 0,   0,   255), // 3: Red
        IM_COL32(102, 0,   153, 255), // 4: Purple
        IM_COL32(255, 127, 0,   255), // 5: Orange
        IM_COL32(0,   143, 0,   255), // 6: Green
        IM_COL32(128, 0,   0,   255), // 7: Maroon
        IM_COL32(20,  20,  20,  255), // 8: Black (Slightly light for visibility)
        IM_COL32(254, 228, 0,   255), // 9: Yellow (S)
        IM_COL32(0,   89,  179, 255), // 10: Blue (S)
        IM_COL32(216, 0,   0,   255), // 11: Red (S)
        IM_COL32(102, 0,   153, 255), // 12: Purple (S)
        IM_COL32(255, 127, 0,   255), // 13: Orange (S)
        IM_COL32(0,   143, 0,   255), // 14: Green (S)
        IM_COL32(128, 0,   0,   255)  // 15: Maroon (S)
    };

    ImU32 baseCol = ballColors[i % 16];
    ImColor col = ImColor(baseCol);

    if (persistent_int[O("iLineStyle")] == 2 && persistent_int.count(O("iColorMode"))) {
        int mode = persistent_int[O("iColorMode")];
        if (mode == 1 && i != 0) { // Bright Mode
            float h, s, v;
            ImGui::ColorConvertRGBtoHSV(col.Value.x, col.Value.y, col.Value.z, h, s, v);
            if (v > 0.1f) {
                if (s > 0.1f) s = ImMin(s * 1.5f, 1.0f); // Boost saturation
                v = ImMin(v * 1.5f, 1.0f); // Boost brightness
            }
            ImGui::ColorConvertHSVtoRGB(h, s, v, col.Value.x, col.Value.y, col.Value.z);
        } else if (mode == 2 && i != 0) { // Cyberpunk Mode
            static const ImU32 cyberpunkColors[16] = {
                IM_COL32(255, 255, 255, 255), // 0: White (Cue)
                IM_COL32(255, 255, 0,   255), // 1: Neon Yellow
                IM_COL32(0,   255, 255, 255), // 2: Electric Cyan (Blue)
                IM_COL32(255, 30,  30,  255), // 3: Neon Red (Red)
                IM_COL32(138, 43,  226, 255), // 4: Blue Violet (Purple)
                IM_COL32(255, 100, 0,   255), // 5: Neon Orange
                IM_COL32(57,  255, 20,  255), // 6: Neon Lime (Green)
                IM_COL32(160, 10,  10,  255), // 7: Dark Neon Maroon (Maroon)
                IM_COL32(110, 110, 110, 255), // 8: Cyber Grey (Black)
                IM_COL32(255, 255, 0,   255), // 9: Neon Yellow
                IM_COL32(0,   255, 255, 255), // 10: Electric Cyan
                IM_COL32(255, 30,  30,  255), // 11: Neon Red
                IM_COL32(138, 43,  226, 255), // 12: Blue Violet
                IM_COL32(255, 100, 0,   255), // 13: Neon Orange
                IM_COL32(57,  255, 20,  255), // 14: Neon Lime
                IM_COL32(160, 10,  10,  255)  // 15: Dark Neon Maroon
            };
            col = ImColor(cyberpunkColors[i % 16]);
        }
    }

    col.Value.w = (isScanning || persistent_bool[O("bESP_ShowBallNumbers")]) ? 1.0f : lineAlpha;
    
    if (i == 0) {
        col = ImColor(255, 255, 255); // Always White for Cue Ball
        col.Value.w = (isScanning || persistent_bool[O("bESP_ShowBallNumbers")]) ? 1.0f : lineAlpha;
    } else if (!isScanning && AutoPlay::g_CurrentCandidate.idx == i) {
        if (persistent_int[O("iLineStyle")] != 2) {
            col = ImColor(255, 215, 0); // Gold for Target when not scanning (unless Colors mode is on)
        }
        col.Value.w = 1.0f;
    }

    return (ImU32)col;
}

static float GetBallLineThickness(int i, bool isScanning) {
    float lineThick = (float)persistent_int[O("iLineThickness")];
    if (isScanning && lineThick < 2.0f) lineThick = 2.0f; // Bolder lines during scan
    else if (lineThick < 1.0f) lineThick = 1.0f;
    
    if (i == 0) {
        if (isScanning) lineThick += 1.0f;
    } else if (!isScanning && AutoPlay::g_CurrentCandidate.idx == i) {
        lineThick += 1.0f;
    }
    return lineThick;
}

INLINE void DrawESP(ImDrawList* draw) {
    if (((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0) || DEBUG_BYPASS_LOGIN) {
        if (!sharedGameManager) return;
        UpdateScreenTable();
        if (!g_Vault.is_loaded) return;

        sharedDirector = F(ptr, libmain + (g_Vault.v[4] ^ XOR_KEY));
        if (!sharedDirector) return;
        sharedUserInfo = F(ptr, libmain + (g_Vault.v[0] ^ XOR_KEY));
        if (!sharedUserInfo) return;
        F(bool, sharedUserInfo + 0x340) = true;
        sharedMainManager = F(ptr, libmain + (g_Vault.v[7] ^ XOR_KEY));
        if (!sharedMainManager) return;

        sharedMenuManager = F(ptr, libmain + (g_Vault.v[2] ^ XOR_KEY));
        if (!sharedMenuManager) return;

        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;
        if (!mainStateManager.isInGame()) {
        if (persistent_bool[O("bAutoQueue")]) {
            if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
        } return;
        }

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto& pockets = tableProperties.mPockets();



        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        AutoPlay::Update();

        auto stateId = gameStateManager.getCurrentStateId();
        if (stateId == 4 && (AutoPlay::currentMode == AutoPlay::MODE_OFF || AutoPlay::state != AutoPlay::SCANNING)) {
            if (AutoPlay::bCueBallIsMovingOrDragging) {
                // Skip simulation while dragging cue ball
            } else if (AutoPlay::state == AutoPlay::NOMINATING || AutoPlay::state == AutoPlay::NOMINATING_HUMAN) {
                // While nominating, force the intended shot line
                gPrediction->determineShotResult(false, AutoPlay::pendingShotAngle, AutoPlay::pendingShotPower, sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            } else if (AutoPlay::g_PredictionLocked && AutoPlay::g_CurrentCandidate.idx != -1) {
                // BOLT LOCK: Keep lines on final shot - don't follow joystick!
                gPrediction->determineShotResult(false, AutoPlay::targetAngle, AutoPlay::pendingShotPower, sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            } else {
                // Normal aim mode - use current cue position
                gPrediction->determineShotResult(false, sharedGameManager.mVisualCue().getShotAngle(), sharedGameManager.mVisualCue().getShotPower(), sharedGameManager.getShotSpin(), AutoPlay::g_CurrentCandidate);
            }
        }
        if (stateId == 6 || stateId == 7 || stateId == 8) return;

        {
            float pocketSize = persistent_float[O("fESP_PocketSize")];
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), pocketSize, IM_COL32(0, 255, 100, 255), 0, 5.0f);
                } else if (persistent_bool[O("bESP_DrawPockets")]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), pocketSize, IM_COL32(255, 180, 0, 255), 0, 3.0f);
                }
            }
        }

        bool showLines = persistent_bool[O("bESP_DrawPredictionLine")];
        if (showLines && !persistent_bool[O("bPredictionAfterShot")]) {
            if (AutoPlay::AreBallsMoving() || !gameStateManager.isPlayerTurn()) {
                showLines = false;
            }
        }

        if (showLines) {
            float lineAlpha = persistent_float[O("fESP_LineAlpha")];
            float ballSize = persistent_float[O("fESP_BallSize")];
            float startPointSize = persistent_float[O("fESP_StartPointSize")];
            bool isScanning = AutoPlay::currentMode != AutoPlay::MODE_OFF && AutoPlay::state == AutoPlay::SCANNING;

            // Pass 1: Draw base solid colored lines for all balls
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImU32 col32 = GetBallColor32(i, lineAlpha, isScanning);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    bool isCyberpunk = (persistent_int[O("iLineStyle")] == 2 &&
                                        persistent_int.count(O("iColorMode")) &&
                                        persistent_int[O("iColorMode")] == 2);

                    // Helper: draw one line segment with optional neon glow
                    auto DrawSegment = [&](ImVec2 a, ImVec2 b) {
                        if (isCyberpunk) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glow1 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 30);
                            ImU32 glow2 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 65);
                            draw->AddLine(a, b, glow1, lineThick * 2.5f); // Thin outer glow
                            draw->AddLine(a, b, glow2, lineThick * 1.5f); // Mid glow
                            draw->AddLine(a, b, col32, lineThick);         // Sharp core
                        } else {
                            draw->AddLine(a, b, col32, lineThick);
                        }
                    };

                    // Helper: fill the crack at a corner joint
                    auto FixJointCrack = [&](ImVec2 p) {
                        if (isCyberpunk) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glow1 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 30);
                            ImU32 glow2 = IM_COL32((int)(c.x*255), (int)(c.y*255), (int)(c.z*255), 65);
                            draw->AddCircleFilled(p, lineThick * 2.5f * 0.5f, glow1);
                            draw->AddCircleFilled(p, lineThick * 1.5f * 0.5f, glow2);
                            draw->AddCircleFilled(p, lineThick * 0.5f, col32);
                        } else {
                            draw->AddCircleFilled(p, lineThick * 0.5f, col32);
                        }
                    };

                    ImVec2 curPos = WorldToScreen(ball.initialPosition);

                    if (ball.positions.size() > 1) {
                        ImVec2 prevDir = {0.0f, 0.0f};
                        for (size_t pi = 1; pi < ball.positions.size(); pi++) {
                            const auto& wp = ball.positions[pi];
                            if (wp == ball.initialPosition && pi == 1) continue; 
                            ImVec2 nextPos = WorldToScreen(wp);
                            float dx = nextPos.x - curPos.x, dy = nextPos.y - curPos.y;
                            float lenSq = dx*dx + dy*dy;
                            if (lenSq >= 2.25f) { 
                                DrawSegment(curPos, nextPos);

                                // Compute current direction
                                float len = sqrtf(lenSq);
                                ImVec2 curDir = {dx / len, dy / len};

                                // Check if direction changed
                                if (prevDir.x != 0.0f || prevDir.y != 0.0f) {
                                    float dotProd = prevDir.x * curDir.x + prevDir.y * curDir.y;
                                    if (dotProd < 0.999f) { // Direction changed (corner)
                                        FixJointCrack(curPos); // Fill the crack exactly at the corner
                                    }
                                    
                                    // Draw user's bounce marker if enabled
                                    if (persistent_bool[O("bESP_BounceMarkers")] && dotProd < 0.96f) {
                                        draw->AddCircleFilled(curPos, 10.0f, col32);
                                    }
                                }

                                prevDir = curDir;
                                curPos = nextPos;
                            }
                        }
                    }

                    // Final segment from last waypoint to predictedPosition
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);
                    if (curPos.x != endPos.x || curPos.y != endPos.y) {
                        DrawSegment(curPos, endPos);
                    }
                }
            }

            // Pass 2: Draw white/black dashed overlays for striped balls (9 to 15) on top of the solid lines
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];
                bool isDashed = (persistent_int[O("iLineStyle")] == 1) && (i >= 9 && i <= 15);

                if (isDashed && ball.initialPosition != ball.predictedPosition) {
                    bool isTargetGold = (!isScanning && AutoPlay::g_CurrentCandidate.idx == i && persistent_int[O("iLineStyle")] != 2);
                    ImU32 dashColor = isTargetGold ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    // Ultra-thin thread look: White dash is roughly 45% of base thickness
                    float whiteThick = ImMax(1.0f, lineThick * 0.45f);

                    ImVec2 curPos = WorldToScreen(ball.initialPosition);
                    float dashPhase = 0.0f; // Carry phase across segments for continuous dashes

                    if (ball.positions.size() > 1) {
                        for (size_t pi = 1; pi < ball.positions.size(); pi++) {
                            const auto& wp = ball.positions[pi];
                            if (wp == ball.initialPosition && pi == 1) continue; 
                            ImVec2 nextPos = WorldToScreen(wp);
                            float dx = nextPos.x - curPos.x, dy = nextPos.y - curPos.y;
                            if (dx*dx + dy*dy >= 2.25f) { 
                                dashPhase = AddDashedLine(draw, curPos, nextPos, dashColor, whiteThick, 22.0f, 18.0f, dashPhase); 
                                curPos = nextPos;
                            }
                        }
                    }
                    // Final segment — continue phase
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);
                    if (curPos.x != endPos.x || curPos.y != endPos.y) {
                        AddDashedLine(draw, curPos, endPos, dashColor, whiteThick, 22.0f, 18.0f, dashPhase);
                    }
                }
            }

            // Pass 3: Draw anchor circles, stop circles, ball numbers, and target indicators on top of everything
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                if (isScanning && !AutoPlay::bShowAutoPlayLines) break;

                auto& ball = gPrediction->guiData.balls[i];

                if (ball.initialPosition != ball.predictedPosition) {
                    ImU32 col32 = GetBallColor32(i, lineAlpha, isScanning);
                    float lineThick = GetBallLineThickness(i, isScanning);
                    bool isDashed = (persistent_int[O("iLineStyle")] == 1) && (i >= 9 && i <= 15);
                    bool isTargetGold = (!isScanning && AutoPlay::g_CurrentCandidate.idx == i && persistent_int[O("iLineStyle")] != 2);
                    ImU32 dashColor = isTargetGold ? IM_COL32(20, 20, 20, 255) : IM_COL32(255, 255, 255, 255);

                    ImVec2 startPos = WorldToScreen(ball.initialPosition);
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);

                    // Anchor circle at start
                    draw->AddCircle(startPos, startPointSize, col32, 0, lineThick);  
                    
                    // Predicted stop position (Filled for Solids/8-ball/Cue, Curved Dashed Hollow for Stripes)
                    if (i >= 9 && i <= 15) {
                        if (isDashed) {
                            // Draw alternating smooth curved dashes (4 colored and 4 white) along the circle boundary using PathArcTo
                            int num_dashes = 4;
                            float segment_angle = (2.0f * 3.14159265f) / (num_dashes * 2);
                            float circleThick = ImMax(1.0f, lineThick * 0.45f); // Slimmer dashed stroke
                            for (int j = 0; j < num_dashes * 2; j++) {
                                float start_a = j * segment_angle;
                                float end_a = (j + 1) * segment_angle;
                                
                                draw->PathClear();
                                draw->PathArcTo(endPos, ballSize, start_a, end_a, 12);
                                ImU32 segCol = (j % 2 == 0) ? col32 : dashColor; // Use dashColor here instead of pure white
                                draw->PathStroke(segCol, false, circleThick);
                            }
                        } else {
                            float ringThick = ImMax(1.5f, lineThick * 0.45f);
                            draw->AddCircle(endPos, ballSize, col32, 0, ringThick);
                        }
                    } else {
                        bool isCyberpunkDot = (persistent_int[O("iLineStyle")] == 2 &&
                                               persistent_int.count(O("iColorMode")) &&
                                               persistent_int[O("iColorMode")] == 2);
                        if (isCyberpunkDot) {
                            ImVec4 c = ImGui::ColorConvertU32ToFloat4(col32);
                            ImU32 glowOut = IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), 30);
                            ImU32 glowMid = IM_COL32((int)(c.x*255),(int)(c.y*255),(int)(c.z*255), 65);
                            draw->AddCircleFilled(endPos, ballSize * 1.25f, glowOut); // Outer glow even smaller
                            draw->AddCircleFilled(endPos, ballSize * 1.1f, glowMid);  // Mid glow even smaller
                            draw->AddCircle(endPos, ballSize + 0.5f, IM_COL32(255,255,255,150), 0, 1.0f); // Thinner white border
                            draw->AddCircleFilled(endPos, ballSize, col32);           // Core dot
                        } else {
                            draw->AddCircleFilled(endPos, ballSize, col32);
                        }
                    }
                    if (persistent_bool[O("bESP_ShowBallNumbers")]) {
                        draw->AddCircle(endPos, ballSize, IM_COL32(0, 0, 0, 255), 0, 1.5f);
                    }

                    // Target indicator ring
                    if (AutoPlay::g_CurrentCandidate.idx == i) {
                        draw->AddCircle(endPos, ballSize * 1.5f, IM_COL32(0, 255, 255, 200), 0, 2.0f);
                    }

                    // Ball Number Overlays
                    if (persistent_bool[O("bESP_ShowBallNumbers")] && i > 0) {
                        char buf[8];
                        sprintf(buf, "%d", i);
                        
                        // Draw a solid white circle slightly smaller than the predicted ball
                        float innerCircleRadius = ballSize * 0.65f;
                        draw->AddCircleFilled(endPos, innerCircleRadius, IM_COL32(255, 255, 255, 255));
                        
                        // Scale down font size slightly
                        ImFont* font = ImGui::GetFont();
                        float fontSize = ImGui::GetFontSize() * 0.75f;
                        
                        // Center the text based on scaled font
                        ImVec2 txtSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
                        ImVec2 txtPos = endPos - (txtSize * 0.5f);
                        
                        // Text is always black inside the white circle
                        draw->AddText(font, fontSize, txtPos, IM_COL32(0, 0, 0, 255), buf);
                    }
                }
            }

            // --- 3. Pocket Pulse Indicator (no duplicate cue→target line: Section 2 already draws the full bounce path) ---
            if (AutoPlay::g_CurrentCandidate.idx != -1 && !isScanning) {
                ImVec2 pocketScreenPos = GetPocketScreenPos(AutoPlay::g_CurrentCandidate.pocketIndex);
                // Animated pulse ring on the target pocket
                float pulse = 5.0f + sin(AutoPlay::nowSec() * 10.0f) * 3.0f;
                draw->AddCircle(pocketScreenPos, pulse, IM_COL32(255, 215, 0, 255), 0, 2.0f);
                draw->AddCircle(pocketScreenPos, ballSize * 1.8f, IM_COL32(0, 255, 255, 180), 0, 1.5f);
            }
        }
    }
}

static void DrawSidebar(float sidebarW) {
    ImGuiContext& g  = *GImGui;
    ImDrawList*   dl = GetWindowDrawList();
    ImVec2        wp = GetWindowPos();

    float closeSize = 35.0f;
    float closeBtnW = 70.0f;
    float tabsW     = sidebarW - closeBtnW;
    float btnW      = tabsW / 5.0f;
    float marginB   = 12.0f;

    // Split channels: 0 = background (drawn last, appears behind), 1 = buttons (drawn first)
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);

    // Draw tab buttons — let ImGui lay them out naturally
    BeginGroup();
    SetCursorPos(ImVec2(0.0f, 0.0f));
    if (SidebarButton(O("Draw"),  "\uF4D7", g_menu.currentTab == 0, btnW)) g_menu.currentTab = 0;
    SameLine(0, 0);
    if (SidebarButton(O("Play"),  "\uF544", g_menu.currentTab == 1, btnW)) g_menu.currentTab = 1;
    SameLine(0, 0);
    if (SidebarButton(O("Queue"), "\uF1DA", g_menu.currentTab == 2, btnW)) g_menu.currentTab = 2;
    SameLine(0, 0);
    if (SidebarButton(O("Extra"), "\uF013", g_menu.currentTab == 3, btnW)) g_menu.currentTab = 3;
    SameLine(0, 0);
    if (SidebarButton(O("Info"),  "\uF05A", g_menu.currentTab == 4, btnW)) g_menu.currentTab = 4;
    EndGroup();

    // Measure actual rendered height — this is the true wrap_content
    float sidebarH = GetItemRectMax().y - wp.y;

    // Now draw background on channel 0 (behind the buttons)
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(wp, ImVec2(wp.x + sidebarW, wp.y + sidebarH), IM_COL32(21, 21, 21, 255), 30.0f);
    dl->ChannelsMerge();

    // Vertical separator between Queue and close strip
    float sepX       = wp.x + sidebarW - closeBtnW;
    float sepCenterY = wp.y + sidebarH * 0.5f;
    float sepHalfH   = sidebarH * 0.28f;
    dl->AddLine(
        ImVec2(sepX, sepCenterY - sepHalfH),
        ImVec2(sepX, sepCenterY + sepHalfH),
        IM_COL32(60, 60, 75, 200), 1.5f
    );

    // Close (X) button — truly centered in the measured sidebarH
    float closePosX = (sidebarW - closeBtnW) + (closeBtnW - closeSize) * 0.5f;
    float closePosY = (sidebarH - closeSize) * 0.5f;
    SetCursorPos(ImVec2(closePosX, closePosY));
    {
        ImGuiWindow* win = GetCurrentWindow();
        ImGuiID closeId  = win->GetID(O("##CloseMenu"));
        ImVec2 closePos  = win->DC.CursorPos;
        ImRect closeBb(closePos, closePos + ImVec2(closeSize, closeSize));
        ItemSize(ImVec2(closeSize, closeSize), g.Style.FramePadding.y);
        ItemAdd(closeBb, closeId);
        bool closeHovered = false, closeHeld = false;
        bool closePressed = ButtonBehavior(closeBb, closeId, &closeHovered, &closeHeld);
        if (closePressed) g_menu.isOpen = false;

        float xCX = closeBb.Min.x + closeSize * 0.5f;
        float xCY = closeBb.Min.y + closeSize * 0.5f;
        float xH  = closeSize * 0.32f;
        ImU32 xCol = closeHovered ? IM_COL32(255, 255, 255, 240) : IM_COL32(160, 160, 170, 200);
        dl->AddLine(ImVec2(xCX - xH, xCY - xH), ImVec2(xCX + xH, xCY + xH), xCol, 2.2f);
        dl->AddLine(ImVec2(xCX + xH, xCY - xH), ImVec2(xCX - xH, xCY + xH), xCol, 2.2f);
    }

    // Bottom margin — cursor pushed past the true sidebar height
    SetCursorPos(ImVec2(0.0f, sidebarH));
    Dummy(ImVec2(sidebarW, marginB));
}

// Reads an IL2CPP/Unity NSString (UTF-16 internal buffer at offset 0x14, length at 0x10)
static std::string ReadNSString(ptr str) {
    if (!str) return "null";
    int32_t len = F(int32_t, str + 0x10);
    if (len <= 0 || len > 512) return "?";
    std::string result;
    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        uint16_t ch = F(uint16_t, str + 0x14 + i * 2);
        result += (ch > 0 && ch < 128) ? (char)ch : '?';
    }
    return result;
}

// Shared vertical position for DrawToggleButton and DrawFloatingButton (they move together)
static float g_sideBtnsY      = 0.0f;
// Kept for linker compatibility — no longer used for animation
static float g_toggleRotAngle = 0.0f;
// Set true by AutoPlay when in SLOW scan state — shows CALCULATING overlay
static bool  g_autoPlayCalculating = false;

// ── svConfig ──────────────────────────────────────────────────────────────────
static void svConfig_Save() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    FILE* f = fopen(path.c_str(), O("w"));
    if (!f) return;
    fprintf(f, O("iLineThickness=%d\n"),  persistent_int[O("iLineThickness")]);
    fprintf(f, O("iMenuSizeOffset=%d\n"), persistent_int[O("iMenuSizeOffset")]);
    fprintf(f, O("iPowerBarSide=%d\n"),   persistent_int[O("iPowerBarSide")]);
    fprintf(f, O("fPowerBarXPercent=%f\n"),      persistent_float[O("fPowerBarXPercent")]);
    fprintf(f, O("fPowerBarYStartPercent=%f\n"), persistent_float[O("fPowerBarYStartPercent")]);
    fprintf(f, O("fPowerBarYEndPercent=%f\n"),   persistent_float[O("fPowerBarYEndPercent")]);
    fclose(f);
}
static void svConfig_Load() {
    std::string path = O("/data/user/0/") + PACKAGE_NAME + O("/files/svConfig.txt");
    persistent_int[O("iPowerBarSide")] = 0; // Default to Left Side
    persistent_float[O("fPowerBarXPercent")] = 0.058f;
    persistent_float[O("fPowerBarYStartPercent")] = 0.32f;
    persistent_float[O("fPowerBarYEndPercent")] = 0.78f;
    FILE* f = fopen(path.c_str(), O("r"));
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int v = 0;
        float fv = 0.0f;
        if (sscanf(line, O("iLineThickness=%d"),  &v) == 1) { persistent_int[O("iLineThickness")]  = v; continue; }
        if (sscanf(line, O("iMenuSizeOffset=%d"), &v) == 1) { persistent_int[O("iMenuSizeOffset")] = v; continue; }
        if (sscanf(line, O("iPowerBarSide=%d"),   &v) == 1) { persistent_int[O("iPowerBarSide")]   = v; continue; }
        if (sscanf(line, O("fPowerBarXPercent=%f"),      &fv) == 1) { persistent_float[O("fPowerBarXPercent")]      = fv; continue; }
        if (sscanf(line, O("fPowerBarYStartPercent=%f"), &fv) == 1) { persistent_float[O("fPowerBarYStartPercent")]  = fv; continue; }
        if (sscanf(line, O("fPowerBarYEndPercent=%f"),    &fv) == 1) { persistent_float[O("fPowerBarYEndPercent")]    = fv; }
    }
    fclose(f);
}

// ── CALCULATING overlay (shown during AutoPlay SLOW scan) ─────────────────────
static void DrawCalculating(ImGuiIO& io) {
    // Setăm poziția pe centrul ecranului (Width*0.5, Height*0.5)
    // Pivotul (0.5f, 0.5f) înseamnă că mijlocul ferestrei va fi fix pe coordonatele date
    SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    // Auto-resize face ca fereastra să aibă dimensiunea textului automat
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleColor(ImGuiCol_Border, IM_COL32(255, 120, 0, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);

    if (Begin(O("##CalcOverlay"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs)) {
        
        SetWindowFontScale(1.4f);
        TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), O("CALCULATING..."));
        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor(2);
}


static void DrawContentArea(float winW, float winH) {
    bool need_save = false;
    
    ImDrawList* dl  = GetWindowDrawList();
    ImVec2      wp  = GetWindowPos();

    // startY este punctul unde se termină bara de butoane (Sidebar)
    float startY   = GetCursorPosY();
    float contentW = winW;

    // Desenăm fundalul zonei de conținut sub sidebar
    dl->AddRectFilled(
        ImVec2(wp.x, wp.y + startY),
        ImVec2(wp.x + contentW, wp.y + winH),
        IM_COL32(21, 21, 21, 255), 20.0f
    );
    
    const char* tabTitles[] = { 
    O("Draw Settings"), 
    O("Auto Play"), 
    O("Auto Queue"), 
    O("Extra Settings"),
    O("Info") 
};

    // --- Premium VIP Header ───────────────────────────────────────────
    float headerH = 95.0f;
    {
        float avail = contentW;
        ImVec2 p = ImVec2(wp.x, wp.y + startY);
        ImDrawList* dl2 = GetWindowDrawList();
        
        int64_t now_ts = (int64_t)time(nullptr);
        int64_t diff = g_ExpiryTime - now_ts;
        if (diff < 0) diff = 0;

        int d = (int)(diff / 86400);
        int h = (int)((diff % 86400) / 3600);
        int m = (int)((diff % 3600) / 60);
        int s = (int)(diff % 60);

        // --- Left Side: Branding ---
        float paddingX = 22.0f;
        float textY = p.y + (headerH - (GetFontSize() * 2.2f)) * 0.5f;
        
        // Primary Text: "Lion X VIP" in Arial Black
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(1.4f);
        ImVec2 textP = ImVec2(p.x + paddingX, textY);
        dl2->AddText(textP, IM_COL32(255, 120, 0, 255), O("Lion X VIP"));
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();

        // Secondary Text: "Active" (Small Light Grey)
        dl2->AddText(ImVec2(p.x + paddingX, textY + GetFontSize() * 1.5f), IM_COL32(180, 180, 190, 255), O("Active"));

        // --- Right Side: Boxed Timer with Colons ---
        auto DrawTimeBox = [&](int value, float& curX, bool drawColonBefore) {
            float boxW = 55.0f;
            float boxH = 50.0f;
            float boxPad = 14.0f; // Extra space to center the colon nicely
            
            curX -= boxW;
            ImVec2 boxP(curX, p.y + (headerH - boxH) * 0.5f);
            
            // Box Background
            dl2->AddRectFilled(boxP, boxP + ImVec2(boxW, boxH), IM_COL32(255, 127, 0, 255), 10.0f);
            
            // Value Text Centered (Restored to clear, prominent size)
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d", value);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            SetWindowFontScale(1.05f); // Clear, legible, beautiful size
            ImVec2 ts = CalcTextSize(buf);
            dl2->AddText(ImVec2(boxP.x + (boxW - ts.x) * 0.5f, boxP.y + (boxH - ts.y) * 0.5f), IM_COL32(255, 255, 255, 255), buf);
            SetWindowFontScale(1.0f);
            if (g_ArialBlackFont) PopFont();
            
            if (drawColonBefore) {
                // Draw colon dots exactly in the center of the gap to the left
                float colonX = curX - (boxPad * 0.5f);
                ImVec2 center(colonX, p.y + headerH * 0.5f);
                dl2->AddCircleFilled(center - ImVec2(0, 5), 2.0f, IM_COL32(255, 255, 255, 160));
                dl2->AddCircleFilled(center + ImVec2(0, 5), 2.0f, IM_COL32(255, 255, 255, 160));
            }
            
            curX -= boxPad; // Apply gap
        };

        float rightX = p.x + avail - paddingX;
        DrawTimeBox(s, rightX, true);  // Colon between seconds and minutes
        DrawTimeBox(m, rightX, true);  // Colon between minutes and hours
        DrawTimeBox(h, rightX, true);  // Colon between hours and days
        DrawTimeBox(d, rightX, false); // Far left (days) - no colon to its left
    }

    SetCursorPos(ImVec2(10.0f, startY + headerH + 10.0f));
    
    // Începutul zonei de child (conținutul propriu-zis)
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    
    // --- Modern Thinner Scrollbar ---
    PushStyleVar(ImGuiStyleVar_ScrollbarSize, 10.0f); // Thinner scrollbar
    PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 12.0f); // Rounded "pill" look
    PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0)); // Hidden track (modern)
    PushStyleColor(ImGuiCol_ScrollbarGrab, IM_COL32(255, 120, 0, 160)); // Orange grab semi-transparent
    PushStyleColor(ImGuiCol_ScrollbarGrabHovered, IM_COL32(255, 140, 30, 220));
    PushStyleColor(ImGuiCol_ScrollbarGrabActive, IM_COL32(255, 120, 0, 255));

    BeginChild(O("##ContentArea"), ImVec2(contentW - 20.0f, winH - startY - headerH - 10.0f), false, ImGuiWindowFlags_NoScrollbar);
    
    // --- God-Tier Smooth Touch Drag Scrolling ---
    {
        ImGuiIO& io = GetIO();
        static bool isScrollingActive = false;
        static bool isSliderDraggingActive = false;

        if (IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
            if (io.MouseDown[0]) {
                if (!isScrollingActive && !isSliderDraggingActive) {
                    float dx = io.MousePos.x - io.MouseClickedPos[0].x;
                    float dy = io.MousePos.y - io.MouseClickedPos[0].y;
                    float distSqr = dx * dx + dy * dy;
                    if (distSqr > 36.0f) { // 6 pixels threshold
                        if (fabs(dy) > fabs(dx)) {
                            isScrollingActive = true;
                            ImGui::ClearActiveID();
                        } else {
                            isSliderDraggingActive = true;
                        }
                    }
                }
                
                if (isScrollingActive) {
                    float deltaY = io.MouseDelta.y;
                    if (deltaY != 0.0f) {
                        SetScrollY(GetScrollY() - deltaY);
                    }
                    ImGui::ClearActiveID();
                }
            } else {
                isScrollingActive = false;
                isSliderDraggingActive = false;
            }
        } else {
            if (!io.MouseDown[0]) {
                isScrollingActive = false;
                isSliderDraggingActive = false;
            }
        }
    }
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Draw Lines"), &persistent_bool[O("bESP_DrawPredictionLine")]); Dummy(ImVec2(0, 8));
            need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPockets")]); Dummy(ImVec2(0, 8));
            // need_save |= ToggleSwitch(O("Internal Line Extension"), &persistent_bool[O("bESP_InternalLineExtension")]); Dummy(ImVec2(0, 8));
            need_save |= ToggleSwitch(O("Show Ball Numbers"), &persistent_bool[O("bESP_ShowBallNumbers")]); Dummy(ImVec2(0, 8));
            need_save |= ToggleSwitch(O("Bounce Sync"), &persistent_bool[O("bESP_BounceMarkers")]);
            
            Dummy(ImVec2(0, 15));
            {
                ImVec2 cp = GetCursorScreenPos();
                dl->AddLine(ImVec2(wp.x + 50.0f, cp.y), ImVec2(wp.x + contentW - 50.0f, cp.y), IM_COL32(255, 120, 0, 100), 1.0f);
            }
            Dummy(ImVec2(0, 15));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Line Style"));
            if (g_ArialBlackFont) PopFont();
            if (!persistent_int.count(O("iLineStyle"))) persistent_int[O("iLineStyle")] = 0; // Default: Default Line
            need_save |= GlassSegmentedControl(O("##line_style"), &persistent_int[O("iLineStyle")], { O("Solid"), O("Stripe"), O("Colors") });
            
            if (persistent_int[O("iLineStyle")] == 2) {
                Dummy(ImVec2(0, 5));
                SetCursorPosX(45.0f);
                if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Color Mode"));
                if (g_ArialBlackFont) PopFont();
                if (!persistent_int.count(O("iColorMode"))) persistent_int[O("iColorMode")] = 0;
                
                need_save |= GlassSegmentedControl(O("##color_mode"), &persistent_int[O("iColorMode")], { O("Default"), O("Vivid"), O("Chroma") });
            }

            Dummy(ImVec2(0, 5));
            if (!persistent_int.count(O("iLineThickness"))) persistent_int[O("iLineThickness")] = 9;
            need_save |= CustomSliderInt(O("Line Thickness"), &persistent_int[O("iLineThickness")], 1, 15);
            need_save |= CustomSliderFloat(O("Line Transparency"), &persistent_float[O("fESP_LineAlpha")], 0.1f, 1.0f);
            need_save |= CustomSliderFloat(O("Pocket Size"), &persistent_float[O("fESP_PocketSize")], 10.0f, 100.0f);
            need_save |= CustomSliderFloat(O("Ball Predicted Size"), &persistent_float[O("fESP_BallSize")], 5.0f, 40.0f);
            need_save |= CustomSliderFloat(O("Start Point Size"), &persistent_float[O("fESP_StartPointSize")], 2.0f, 40.0f);
            need_save |= CustomSliderInt(O("Fix Menu Size"), &persistent_int[O("iMenuSizeOffset")], -10, 10);

            Dummy(ImVec2(0, 20));
            {
                float btnH = 80.0f;
                float sideMargin = contentW * 0.15f; // More centered look
                float avail = (contentW - 20.0f) - (sideMargin * 2.0f);
                ImVec2 p = GetCursorScreenPos();
                p.x += sideMargin;
                ImVec2 size = ImVec2(avail, btnH);
                
                ImGuiID id = GetID(O("SaveBtn_Premium"));
                bool hovered, held;
                bool pressed = ButtonBehavior(ImRect(p, p + size), id, &hovered, &held);
                
                // Animation state
                static std::map<ImGuiID, float> btnAnim;
                if (btnAnim.find(id) == btnAnim.end()) btnAnim[id] = 0.0f;
                btnAnim[id] = ImLerp(btnAnim[id], hovered ? 1.0f : 0.0f, GetIO().DeltaTime * 8.0f);
                float anim = btnAnim[id];

                ImDrawList* dl2 = GetWindowDrawList();
                
                // 1. Layer: Shadow / Outer Glow
                ImU32 shadowCol = IM_COL32(255, 120, 0, (int)(40 * anim));
                float rounding = 25.0f; // Increased for a smoother look
                for(int i=1; i<=8; i++) 
                    dl2->AddRect(p - ImVec2(i,i), p + size + ImVec2(i,i), IM_COL32(255, 100, 0, (int)((20-i*2)*anim)), rounding + i, 0, 1.0f);

                // 2. Layer: Main Body
                ImU32 colBody = hovered ? IM_COL32(255, 130, 20, 255) : IM_COL32(255, 110, 0, 255);
                if (held) colBody = IM_COL32(180, 70, 0, 255);
                dl2->AddRectFilled(p, p + size, colBody, rounding);

                // 3. Layer: Glossy Top Edge
                dl2->AddLine(p + ImVec2(30, 2), p + ImVec2(size.x - 30, 2), IM_COL32(255, 255, 255, 90), 1.5f);

                // 4. Layer: Icon & Text
                float iconSize = 30.0f;
                float spacing = 15.0f;
                ImVec2 ts = CalcTextSize(O("SAVE CHANGES"));
                float totalW = iconSize + spacing + ts.x;
                ImVec2 contentStart = ImVec2(p.x + (avail - totalW) * 0.5f, p.y + (btnH - iconSize) * 0.5f);

                // Vector Disk Icon
                ImVec2 iP = contentStart;
                dl2->AddRectFilled(iP, iP + ImVec2(iconSize, iconSize), IM_COL32(255, 255, 255, 220), 4.0f); // Body
                dl2->AddRectFilled(iP + ImVec2(6, 2), iP + ImVec2(iconSize - 6, 12), IM_COL32(40, 40, 50, 255), 2.0f); // Top part
                dl2->AddRectFilled(iP + ImVec2(8, 18), iP + ImVec2(iconSize - 8, iconSize - 2), IM_COL32(60, 60, 70, 255), 1.0f); // Shutter
                dl2->AddRect(iP, iP + ImVec2(iconSize, iconSize), IM_COL32(0, 0, 0, 40), 4.0f); // Outline

                // Text
                dl2->AddText(ImVec2(contentStart.x + iconSize + spacing, p.y + (btnH - ts.y) * 0.5f), IM_COL32(255, 255, 255, 255), O("SAVE CHANGES"));
                
                if (pressed) {
                    svConfig_Save();
                }
                Dummy(size);
                Dummy(ImVec2(0, 10));
            }
            break;
        }
        
        case 1: {
            Dummy(ImVec2(0, 10));
            
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("AUTOMATION CONTROL"));
            if (g_ArialBlackFont) PopFont();
            if (ToggleSwitch(O("Auto Play Mode"), &persistent_bool[O("bAutoPlaySwitch")])) {
                AutoPlay::bAutoPlaySwitch = persistent_bool[O("bAutoPlaySwitch")];
                if (AutoPlay::bAutoPlaySwitch) {
                    persistent_bool[O("bAutoAimSwitch")] = false;
                    AutoPlay::bAutoAimSwitch = false;
                }
                AutoPlay::bAutoPlaying = false; 
                AutoPlay::currentMode = AutoPlay::MODE_OFF;
                AutoPlay::ClearState();
                need_save = true;
            }
            Dummy(ImVec2(0, 8));
            if (ToggleSwitch(O("Auto Aim Mode"), &persistent_bool[O("bAutoAimSwitch")])) {
                AutoPlay::bAutoAimSwitch = persistent_bool[O("bAutoAimSwitch")];
                if (AutoPlay::bAutoAimSwitch) {
                    persistent_bool[O("bAutoPlaySwitch")] = false;
                    AutoPlay::bAutoPlaySwitch = false;
                }
                AutoPlay::bAutoPlaying = false; 
                AutoPlay::currentMode = AutoPlay::MODE_OFF;
                AutoPlay::ClearState();
                need_save = true;
            }
            
            Dummy(ImVec2(0, 12));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Automation Speed"));
            if (g_ArialBlackFont) PopFont();
            if (!persistent_int.count(O("iAutoSpeed"))) persistent_int[O("iAutoSpeed")] = 0; // Default Maximum Fast
            std::vector<std::string> speeds = { O("Maximum Fast"), O("Minimum Slow") };
            need_save |= GlassSegmentedControl("##auto_speed", &persistent_int[O("iAutoSpeed")], speeds);
            AutoPlay::automationSpeed = (AutoPlay::AutomationSpeed)persistent_int[O("iAutoSpeed")];

            Dummy(ImVec2(0, 12));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Play Style"));
            if (g_ArialBlackFont) PopFont();
            if (!persistent_int.count(O("iPlayStyle"))) persistent_int[O("iPlayStyle")] = 0; // Default Natural Play
            std::vector<std::string> styles = { O("Natural Play"), O("Instant Mode") };
            need_save |= GlassSegmentedControl("##play_style", &persistent_int[O("iPlayStyle")], styles);
            AutoPlay::playStyle = (AutoPlay::PlayStyle)persistent_int[O("iPlayStyle")];
 
            Dummy(ImVec2(0, 15));
            
            // Clean Table as a simple Toggle Switch (Default OFF = YOUR BALLS, ON = ALL BALLS)
            if (!persistent_bool.count(O("bCleanTable"))) persistent_bool[O("bCleanTable")] = false;
            // HIDDEN FOR NEXT UPDATE: need_save |= ToggleSwitch(O("Clean Table"), &persistent_bool[O("bCleanTable")]);
            AutoPlay::cleanTableMode = persistent_bool[O("bCleanTable")] ? AutoPlay::CLEAN_ALL_BALLS : AutoPlay::CLEAN_YOUR_BALLS;
 
            Dummy(ImVec2(0, 12));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("9-Ball Strategy"));
            if (g_ArialBlackFont) PopFont();
            if (!persistent_int.count(O("iNineMode"))) persistent_int[O("iNineMode")] = 1; // Default Snipe 9
            std::vector<std::string> nineModes = { O("Best Shot"), O("Snipe 9") };
            need_save |= GlassSegmentedControl("##nine_mode", &persistent_int[O("iNineMode")], nineModes);
            AutoPlay::nineBallStrategy = (AutoPlay::NineBallStrategy)(persistent_int[O("iNineMode")] + 1);
 
            Dummy(ImVec2(0, 15));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("AUTO PLAY PHYSICS"));
            if (g_ArialBlackFont) PopFont();
            
            // Auto Spin Control Hidden as requested
            AutoPlay::bAutoSpin = false;
            
            /*
            need_save |= ToggleSwitch(O("Auto Spin Control"), &persistent_bool[O("bAutoSpin")]);
            AutoPlay::bAutoSpin = persistent_bool[O("bAutoSpin")];
            
            if (AutoPlay::bAutoSpin) {
                Dummy(ImVec2(0, 8));
                std::vector<std::string> spins = { O("Top"), O("Bottom"), O("Left"), O("Right"), O("Center") };
                need_save |= SegmentedControl("##spin_preset", &persistent_int[O("iSpinPreset")], spins, 65.0f);
                AutoPlay::spinPreset = (AutoPlay::SpinPreset)persistent_int[O("iSpinPreset")];
            }
            */


            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Auto Call Pocket"), &persistent_bool[O("bAutoPocket")]);
            if (!persistent_bool.count(O("bAutoPocket"))) persistent_bool[O("bAutoPocket")] = true; // Default ON
            AutoPlay::bAutoPocket = persistent_bool[O("bAutoPocket")];

            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Cushion Shot (8-Ball)"), &persistent_bool[O("bCushionShot")]);
            if (!persistent_bool.count(O("bCushionShot"))) persistent_bool[O("bCushionShot")] = true; // Default ON
            AutoPlay::bCushionShot = persistent_bool[O("bCushionShot")];

            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Show Pocket Target Visual"), &persistent_bool[O("bPocketTargetVisual")]);
            if (!persistent_bool.count(O("bPocketTargetVisual"))) persistent_bool[O("bPocketTargetVisual")] = true; // Default ON

            Dummy(ImVec2(0, 10));
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Power Bar Side"));
            if (g_ArialBlackFont) PopFont();
            if (!persistent_int.count(O("iPowerBarSide"))) persistent_int[O("iPowerBarSide")] = 0; // Default: Left Side
            std::vector<std::string> powerBarSides = { O("Left Side"), O("Right Side") };
            need_save |= GlassSegmentedControl("##powerbar_side", &persistent_int[O("iPowerBarSide")], powerBarSides);

            if (!persistent_float.count(O("fPowerBarXPercent"))) persistent_float[O("fPowerBarXPercent")] = 0.058f;
            if (!persistent_float.count(O("fPowerBarYStartPercent"))) persistent_float[O("fPowerBarYStartPercent")] = 0.32f;
            if (!persistent_float.count(O("fPowerBarYEndPercent"))) persistent_float[O("fPowerBarYEndPercent")] = 0.78f;
            // Dummy(ImVec2(0, 15));
            // SetCursorPosX(45.0f);
            // TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("POWER BAR CALIBRATION"));
            // need_save |= CustomSliderFloat(O("Slider X Position %"), &persistent_float[O("fPowerBarXPercent")], 0.010f, 0.200f, "%.3f");
            // need_save |= CustomSliderFloat(O("Slider Y Start %"), &persistent_float[O("fPowerBarYStartPercent")], 0.100f, 0.500f, "%.3f");
            // need_save |= CustomSliderFloat(O("Slider Y End %"), &persistent_float[O("fPowerBarYEndPercent")], 0.500f, 0.950f, "%.3f");

            Dummy(ImVec2(0, 10));
            break;
        }
        
        case 2: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable AutoQueue"), &persistent_bool[O("bAutoQueue")]);
            Dummy(ImVec2(0, 20));
            
            std::vector<std::string> qModes = {O("Last Selected"), O("Smart"), O("Fix Table")};
            need_save |= GlassSegmentedControl(O("##mode_q"), &persistent_int[O("iAutoQueue_Mode")], qModes, 90.0f);
            
            if (persistent_int[O("iAutoQueue_Mode")] == 1) {
                Dummy(ImVec2(0, 15));
                need_save |= CustomSliderInt(O("Bet Percent"), &persistent_int[O("iAutoQueue_BetPercent")], 1, 100);
            }

            if (persistent_int[O("iAutoQueue_Mode")] == 2) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Select Table"));
                Dummy(ImVec2(0, 8));

                static const char* tableLabels[] = {
                    "100", "200", "1k", "2.5k", "10k", "50k", "100k", "500k",
                    "1M", "2M", "5M", "8M", "10M", "20M", "30M", "50M", "200M"
                };

                int& selected = persistent_int[O("iAutoQueue_FixTable")];
                float avail   = GetContentRegionAvail().x;
                int   cols    = 3; // Reduced to 3 columns for better mobile tap target size
                float gap     = 10.0f;
                float btnW    = (avail - gap * (cols - 1)) / cols;
                float btnH    = 100.0f; // Increased height significantly as requested

                PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

                static std::map<int, float> tileAnim;
                for (int i = 0; i < 17; i++) {
                    if (i % cols != 0) SameLine(0, gap);

                    ImVec2 p = GetCursorScreenPos();
                    ImVec2 size = ImVec2(btnW, btnH);
                    
                    char btnId[64];
                    snprintf(btnId, sizeof(btnId), "%s##table_tile_%d", tableLabels[i], i);
                    ImGuiID id = GetID(btnId);
                    
                    // Add a deadzone margin so touching the edges of the button allows scrolling instead of clicking
                    ImRect btnBb(p + ImVec2(15.0f, 15.0f), p + size - ImVec2(15.0f, 15.0f));
                    bool hovered, held;
                    bool pressed = ButtonBehavior(btnBb, id, &hovered, &held);

                    if (tileAnim.find(i) == tileAnim.end()) tileAnim[i] = 0.0f;
                    tileAnim[i] = ImLerp(tileAnim[i], hovered ? 1.0f : 0.0f, GetIO().DeltaTime * 10.0f);
                    
                    bool isSel = (selected == i);
                    ImDrawList* dl2 = GetWindowDrawList();
                    float rounding = 25.0f; // Matching the stylish Save button

                    // 1. Layer: Backdrop (Rounded Selection Fix)
                    if (isSel) {
                        // Fixed: Using AddRectFilled with rounding instead of non-rounded GradientRect
                        ImU32 colBody = hovered ? IM_COL32(255, 140, 0, 255) : IM_COL32(255, 110, 0, 255);
                        if (held) colBody = IM_COL32(180, 70, 0, 255);
                        dl2->AddRectFilled(p, p + size, colBody, rounding);
                        
                        // Premium Glow Bloom
                        for(int j=1; j<=5; j++) 
                            dl2->AddRect(p - ImVec2(j,j), p + size + ImVec2(j,j), IM_COL32(255, 100, 0, 12 - j*2), rounding + j, 0, 1.0f);
                        
                        // Glossy Top Detail
                        dl2->AddLine(p + ImVec2(rounding, 2), p + ImVec2(btnW - rounding, 2), IM_COL32(255, 255, 255, 80), 1.0f);
                    } else {
                        // Unselected: Soft Glass
                        ImU32 bgCol = IM_COL32(45, 45, 55, 180);
                        dl2->AddRectFilled(p, p + size, bgCol, rounding);
                        if (hovered) dl2->AddRect(p, p + size, IM_COL32(255, 255, 255, 40), rounding, 0, 1.5f);
                    }

                    // 2. Layer: Centered Content (Stacked Coins + Label)
                    float coinR = 11.0f;
                    float coinStackOffset = 5.0f;
                    float spacing = 12.0f;
                    ImVec2 ts = CalcTextSize(tableLabels[i]);
                    float totalContentW = (coinR * 2 + coinStackOffset) + spacing + ts.x;
                    float contentStartX = p.x + (btnW - totalContentW) * 0.5f;
                    float centerY = p.y + btnH * 0.5f;

                    // Draw Stacked Golden Coins
                    // -- Back Coin --
                    ImVec2 coinP1 = ImVec2(contentStartX + coinR, centerY + 2);
                    dl2->AddCircleFilled(coinP1, coinR, IM_COL32(180, 120, 0, 255), 0); // Darker base
                    dl2->AddCircle(coinP1, coinR, IM_COL32(140, 90, 0, 255), 0, 1.0f);

                    // -- Front Coin --
                    ImVec2 coinP2 = ImVec2(contentStartX + coinR + coinStackOffset, centerY - 2);
                    dl2->AddCircleFilled(coinP2, coinR, IM_COL32(255, 215, 0, 255), 0); // Golden bright
                    dl2->AddCircle(coinP2, coinR, IM_COL32(200, 160, 0, 255), 0, 1.2f);
                    // Shine detail on top coin
                    dl2->AddCircle(coinP2, coinR * 0.6f, IM_COL32(255, 240, 150, 200), 0, 1.0f);

                    // 3. Layer: Label (Line-up with coins)
                    dl2->AddText(ImVec2(contentStartX + (coinR * 2 + coinStackOffset) + spacing, centerY - ts.y * 0.5f), 
                        isSel ? IM_COL32(255, 255, 255, 255) : IM_COL32(220, 220, 230, 255), tableLabels[i]);

                    if (pressed) {
                        selected = i;
                        need_save = true;
                    }
                    Dummy(size);
                }
                PopStyleVar();
            }

            if (persistent_int[O("iAutoQueue_Mode")] == 0) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("You will be auto queued to"));
                TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("the last game mode you played"));
            }
            break;
        }

        case 3: {
            Dummy(ImVec2(0, 10));
            
            SetCursorPosX(45.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("EXTRA FEATURES"));
            if (g_ArialBlackFont) PopFont();
            Dummy(ImVec2(0, 5));

            if (!persistent_bool.count(O("bDisableAds"))) persistent_bool[O("bDisableAds")] = true; // Default ON
            need_save |= ToggleSwitch(O("Disable Ads"), &persistent_bool[O("bDisableAds")]);
            Dummy(ImVec2(0, 8));
            
            need_save |= ToggleSwitch(O("Hide Resolution Text"), &persistent_bool[O("bHideResText")]);
            Dummy(ImVec2(0, 8));

            need_save |= ToggleSwitch(O("FPS Counter"), &persistent_bool[O("bFpsCounter")]);
            
            Dummy(ImVec2(0, 10));

            if (!persistent_float.count(O("fScannerLevel"))) persistent_float[O("fScannerLevel")] = 50.0f;
            need_save |= CustomSliderFloat(O("Scanner Level"), &persistent_float[O("fScannerLevel")], 0.0f, 100.0f, "%.0f%%");

            Dummy(ImVec2(0, 8));

            if (!persistent_float.count(O("fPredictionFps"))) persistent_float[O("fPredictionFps")] = 30.0f;
            need_save |= CustomSliderFloat(O("Prediction FPS"), &persistent_float[O("fPredictionFps")], 15.0f, 30.0f, "%.0f FPS");

            if (!persistent_bool.count(O("bPredictionAfterShot"))) persistent_bool[O("bPredictionAfterShot")] = false;
            need_save |= ToggleSwitch(O("Prediction After Shot"), &persistent_bool[O("bPredictionAfterShot")]);

            Dummy(ImVec2(0, 8));

            if (!persistent_bool.count(O("bDisableFlicker"))) persistent_bool[O("bDisableFlicker")] = false;
            need_save |= ToggleSwitch(O("Disable Flicker"), &persistent_bool[O("bDisableFlicker")]);

            Dummy(ImVec2(0, 8));

            break;
        }

        case 4: {
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            // ── helpers ──────────────────────────────────────────────────────
            auto DrawSectionHeader = [&](const char* title) {
                Dummy(ImVec2(0, 14));
                float avail = GetContentRegionAvail().x;
                ImVec2 p    = GetCursorScreenPos();
                float  fs   = GImGui->FontSize;
                ImVec2 ts   = CalcTextSize(title);
                float  lineY = p.y + fs * 0.5f;
                float  gap   = 8.0f;
                float  lineW = (avail - ts.x - gap * 2.0f) * 0.5f;
                ImDrawList* dl2 = GetWindowDrawList();
                dl2->AddLine(ImVec2(p.x,                      lineY), ImVec2(p.x + lineW,                      lineY), IM_COL32(60,60,75,160), 1.0f);
                dl2->AddLine(ImVec2(p.x + lineW + gap + ts.x + gap, lineY), ImVec2(p.x + avail, lineY), IM_COL32(60,60,75,160), 1.0f);
                SetCursorPosX(GetCursorPosX() + lineW + gap);
                TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f), "%s", title);
                Dummy(ImVec2(0, 6));
            };

            auto DrawInfoRow = [&](const char* key, const char* val, ImVec4 valCol = ImVec4(0.90f, 0.90f, 0.95f, 1.0f)) {
                SetCursorPosX(35.0f); // Move away from the left edge
                TextColored(ImVec4(0.65f, 0.65f, 0.75f, 1.0f), "%s", key);
                SameLine(210.0f);    // Increased spacing for a cleaner look
                
                if (strstr(key, "Key")) {
                    SetWindowFontScale(1.15f); // Unique font look for the key
                    TextColored(valCol, "%s", val);
                    SetWindowFontScale(1.0f);
                } else {
                    TextColored(valCol, "%s", val);
                }
                Dummy(ImVec2(0, 4));
            };

            Dummy(ImVec2(0, 5));

          /*  // ── User Game Info ────────────────────────────────────────────────
            DrawSectionHeader(O("User Game Info"));

            if (sharedUserInfo) {
                DrawInfoRow(O("Coins:        "), ReadNSString(sharedUserInfo.coins()).c_str());
                DrawInfoRow(O("Cash:         "), ReadNSString(sharedUserInfo.cash()).c_str());
                DrawInfoRow(O("Display Name: "), ReadNSString(sharedUserInfo.DisplayName()).c_str());
                DrawInfoRow(O("Country Code: "), ReadNSString(sharedUserInfo.loginCountryCode()).c_str());
            } else {
                TextColored(ImVec4(0.6f, 0.3f, 0.3f, 1.0f), O("UserInfo not available"));
            }*/

            // ── Colors ───────────────────────────────────────────────────────
            ImVec4 green(0.0f, 1.0f, 0.4f, 1.0f);
            ImVec4 yellow(1.0f, 0.9f, 0.0f, 1.0f);
            ImVec4 red(1.0f, 0.2f, 0.2f, 1.0f);
            ImVec4 gold(1.0f, 0.84f, 0.0f, 1.0f);
            ImVec4 cyan(0.0f, 1.0f, 1.0f, 1.0f);

            SetWindowFontScale(1.15f);
            DrawSectionHeader(O("Information"));
            {
                static char s_manufacturer[PROP_VALUE_MAX] = {};
                static char s_model[PROP_VALUE_MAX]        = {};
                static char s_abi[PROP_VALUE_MAX]          = {};
                static bool s_props_loaded = false;
                if (!s_props_loaded) {
                    __system_property_get("ro.product.manufacturer", s_manufacturer);
                    __system_property_get("ro.product.model",        s_model);
                    __system_property_get("ro.product.cpu.abi",      s_abi);
                    s_props_loaded = true;
                }

                DrawInfoRow(O("Device: "), s_manufacturer, green);
                DrawInfoRow(O("Model:  "), s_model, green);
                DrawInfoRow(O("ABI:    "), s_abi, yellow);
                DrawInfoRow(O("Game:   "), O("8ball pool 56.23.2"), red);
                DrawInfoRow(O("Type:   "), O("LionX Mode"), red);
                Dummy(ImVec2(0, 5));
            }

            // ── License Info Section ──────────────────────────────────────────
            DrawSectionHeader(O("License Info"));
            {
                float avail = GetContentRegionAvail().x;
                
                DrawInfoRow(O("Status: "), O("Activated"), cyan);
                DrawInfoRow(O("Key:    "), g_Key.c_str(), green);
                DrawInfoRow(O("Seller: "), g_Reseller.c_str(), gold);
                
                if (g_isTrial) {
                    Dummy(ImVec2(0, 10));
                    PushTextWrapPos(avail - 40);
                    TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), O("This key is provied for free. Any attempt to sell it should be considered a sacm"));
                    PopTextWrapPos();
                }
                
                Dummy(ImVec2(0, 10));
            }
            SetWindowFontScale(1.0f);
            if (g_ArialBlackFont) PopFont();
            break;
        }
    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleColor(5); // ChildBg, ScrollbarBg, Grab, Hovered, Active
    PopStyleVar(2);   // ScrollbarSize, ScrollbarRounding
}

struct ToastMsg {
    std::string text;
    float alpha;
    float targetY;
    float currentY;
    double spawnTime;
};
static std::vector<ToastMsg> g_Toasts;
static double g_lastToastTime = 0.0;
static int g_toastState = 0; // 0: idle, 1: scanning, 2: found

static void PushToast(const std::string& text) {
    for (auto& t : g_Toasts) {
        t.targetY -= 85.0f; // Shift older toasts up higher to avoid overlap with bigger boxes
    }
    ToastMsg t;
    t.text = text;
    t.alpha = 0.0f;
    t.targetY = ImGui::GetIO().DisplaySize.y - 140.0f; // Lifted much higher to avoid bottom screen/nav bar clipping
    t.currentY = t.targetY + 40.0f; // Start further down for a smoother slide-up spawn
    t.spawnTime = ImGui::GetTime();
    g_Toasts.push_back(t);
    g_lastToastTime = ImGui::GetTime();
}

static void DrawToasts(ImDrawList* draw) {
    if (!persistent_bool[O("bDisableFlicker")]) {
        if (!g_Toasts.empty()) { g_Toasts.clear(); g_toastState = 0; }
        return;
    }

    double now = ImGui::GetTime();
    bool isScanning = (AutoPlay::currentMode != AutoPlay::MODE_OFF && AutoPlay::state == AutoPlay::SCANNING);
    bool isFound = (AutoPlay::g_CurrentCandidate.idx != -1);
    
    if (isFound) {
        if (g_toastState != 2) {
            PushToast(O("Shot Found!"));
            g_toastState = 2;
        } else if (now - g_lastToastTime > 1.5) {
            g_Toasts.clear();
        }
    } else if (isScanning) {
        if (g_toastState != 1 || now - g_lastToastTime > 1.2) { // Slightly longer interval between toasts
            if (g_toastState != 1) {
                PushToast(O("Scanning..."));
            } else {
                int r = rand() % 2;
                PushToast(r == 0 ? O("Scanning...") : O("Finding Shot..."));
            }
            g_toastState = 1;
        }
    } else {
        if (g_toastState == 1) {
            g_Toasts.clear();
            g_toastState = 0;
        }
    }

    float dt = ImGui::GetIO().DeltaTime;
    
    // Animate and draw toasts
    for (int i = 0; i < (int)g_Toasts.size(); i++) {
        auto& t = g_Toasts[i];
        
        bool isLatest = (i == (int)g_Toasts.size() - 1);
        
        if (!isLatest || (now - t.spawnTime > 1.5 && g_toastState != 2)) {
            t.alpha -= dt * 2.5f; // Fade out older or expiring toasts
        } else {
            t.alpha += dt * 4.0f; // Fade in latest
        }
        if (t.alpha > 1.0f) t.alpha = 1.0f;
        
        t.currentY += (t.targetY - t.currentY) * 6.0f * dt; // Lowered from 12.0f for a much smoother glide
        
        if (t.alpha <= 0.01f) {
            g_Toasts.erase(g_Toasts.begin() + i);
            i--;
            continue;
        }
        
        ImFont* font = ImGui::GetFont();
        float fontSize = font->FontSize * 1.2f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.text.c_str());
        
        float paddingX = 36.0f; // Width reduced as requested
        float paddingY = 22.0f; // Height increased as requested
        float boxW = textSize.x + paddingX * 2.0f;
        float boxH = textSize.y + paddingY * 2.0f;
        
        float x = ImGui::GetIO().DisplaySize.x - boxW - 30.0f;
        float y = t.currentY;
        
        ImU32 bgCol = IM_COL32(0, 0, 0, (int)(t.alpha * 220)); // Deep black bg
        ImU32 textCol = IM_COL32(255, 255, 255, (int)(t.alpha * 255));
        
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + boxW, y + boxH), bgCol, 6.0f);
        
        // Running Animated Border Effect
        float t_anim = (float)now * 2.0f; // Speed of animation
        ImVec2 p_min = ImVec2(x, y);
        ImVec2 p_max = ImVec2(x + boxW, y + boxH);
        float perimeter = (boxW * 2.0f) + (boxH * 2.0f);
        float runLen = 60.0f; // Length of the running border highlight
        
        // Draw static dim orange border as base
        draw->AddRect(p_min, p_max, IM_COL32(100, 50, 0, (int)(t.alpha * 255)), 6.0f, 0, 2.0f);
        
        // Draw the moving bright segment using Path API for smooth clipping around corners
        float offset = fmodf(t_anim * 150.0f, perimeter); // Distance traveled
        
        // Setup clip rect for the glowing segment to simulate a running line
        ImVec2 glowMin = p_min;
        ImVec2 glowMax = p_max;
        
        // Simple 4-edge perimeter tracking for the highlight
        if (offset < boxW) { // Top edge
            glowMin.x = p_min.x + offset - runLen;
            glowMax.x = p_min.x + offset;
            glowMin.y = p_min.y - 8.0f;
            glowMax.y = p_min.y + 8.0f;
        } else if (offset < boxW + boxH) { // Right edge
            glowMin.x = p_max.x - 8.0f;
            glowMax.x = p_max.x + 8.0f;
            glowMin.y = p_min.y + (offset - boxW) - runLen;
            glowMax.y = p_min.y + (offset - boxW);
        } else if (offset < boxW * 2.0f + boxH) { // Bottom edge
            glowMin.x = p_max.x - (offset - boxW - boxH);
            glowMax.x = p_max.x - (offset - boxW - boxH) + runLen;
            glowMin.y = p_max.y - 8.0f;
            glowMax.y = p_max.y + 8.0f;
        } else { // Left edge
            glowMin.x = p_min.x - 8.0f;
            glowMax.x = p_min.x + 8.0f;
            glowMin.y = p_max.y - (offset - boxW * 2.0f - boxH);
            glowMax.y = p_max.y - (offset - boxW * 2.0f - boxH) + runLen;
        }
        
        draw->PushClipRect(ImVec2(ImMax(p_min.x, glowMin.x), ImMax(p_min.y, glowMin.y)), 
                           ImVec2(ImMin(p_max.x, glowMax.x), ImMin(p_max.y, glowMax.y)), true);
        draw->AddRect(p_min, p_max, IM_COL32(255, 200, 0, (int)(t.alpha * 255)), 6.0f, 0, 2.0f); // Bright Yellow/Orange Highlight
        draw->PopClipRect();

        // Draw Text
        draw->AddText(font, fontSize, ImVec2(x + paddingX, y + paddingY), textCol, t.text.c_str(), NULL, 0.0f, NULL);
    }
}

INLINE void DrawMenu(ImGuiIO& io) {
    if (((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0) || (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) {
                ImDrawList* bgDraw = GetBackgroundDrawList();
                DrawESP(bgDraw);
                DrawToasts(bgDraw);
            }
            jump_buffer_active = 0;
        }

        float targetAlpha = g_menu.isOpen ? 1.0f : 0.0f;
        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            float sizeScale = 1.0f + (float)persistent_int[O("iMenuSizeOffset")] * 0.03f;
            if (sizeScale < 0.3f) sizeScale = 0.3f;
            float winW = g_menu.sidebarW * sizeScale;
            float winH = 625.0f * sizeScale; // Set to 625f per user request
            
            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            // Anchor top at the exact original position (Center - Half of 618f)
            // This keeps the header in the old 'centered 618' spot, but allows 625+ expansion downwards.
            SetNextWindowPos(ImVec2(Width / 2.0f, (Height / 2.0f) - (618.0f * 0.5f * sizeScale)), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.13f, 0.f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);
            
            ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            
            if (Begin(O("##MainMenu"), &g_menu.isOpen, winFlags)) {
                DrawSidebar(winW);
                DrawContentArea(winW, winH);
            }
            End();
            
            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

// Moved from AutoPlay namespace — plays/pauses autoplay (cancelMode=false)
// or cancels autoqueue (cancelMode=true)
static void DrawToggleButton(bool cancelMode) {
    if (!cancelMode && !AutoPlay::bAutoPlaySwitch && !AutoPlay::bAutoAimSwitch) {
        AutoPlay::bAutoPlaying = false;
        AutoPlay::currentMode = AutoPlay::MODE_OFF;
        return;
    }
    ImGuiIO& io = GetIO();

    static GLuint play_on_tex       = LoadTextureFromMemory(play_on_png,       play_on_png_len);
    static GLuint play_off_tex      = LoadTextureFromMemory(play_off_png,       play_off_png_len);
    static GLuint queue_cancel_tex  = LoadTextureFromMemory(play_on_png,   play_on_png_len);

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    float button_size   = 130.0f * scale;
    float winSize       = 155.0f * scale; // Match DrawFloatingButton's window size
    const float rightMargin = 20.0f * scale;
    float fixedX = io.DisplaySize.x - rightMargin - winSize; 

    float windowWidth   = button_size + 20.0f * scale;
    float windowHeight  = button_size + 20.0f * scale;
    
    // Adjust fixedX to center the smaller 130px button under the 150px main button
    float buttonX = fixedX + (winSize - windowWidth) * 0.5f;

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 120.0f; // Default starting height

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(buttonX, g_sideBtnsY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f)); // Add padding to prevent clipping

    if (Begin(O("##ToggleBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {

        ImVec2 pos    = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

        bool clicked = InvisibleButton(O("##TglBtnHit"), size);

        float r = size.x * 0.5f;
        ImDrawList* dl = GetWindowDrawList();
        
        // Circular Background for Toggle
        dl->AddCircleFilled(center, r, IM_COL32(18, 18, 22, 255));
        
        // Stylish Glow (Static)
        dl->AddCircle(center, r + 2.0f, IM_COL32(255, 120, 0, 100), 0, 3.0f);
        
        // Circular Border for Toggle - Static thickness, no wobble
        dl->AddCircle(center, r, IM_COL32(255, 120, 0, 255), 0, 4.5f);

        // Inner Animated Circle
        static float innerCircleAnim = 0.0f;
        float targetAnim = AutoPlay::bAutoPlaying ? 1.0f : 0.0f;
        innerCircleAnim += (targetAnim - innerCircleAnim) * io.DeltaTime * 12.0f;
        
        float baseInnerR = r - 16.0f; 
        float expandedInnerR = r; // Mix with main border when playing
        float currentInnerR = baseInnerR + (expandedInnerR - baseInnerR) * innerCircleAnim;
        
        // Halka (light/semi-transparent) inner circle
        dl->AddCircle(center, currentInnerR, IM_COL32(255, 180, 50, 150), 0, 2.0f);
        
        // Draw Play/Pause Symbols manually
        if (AutoPlay::bAutoPlaying) {
            // Pause Symbol (Two Vertical Bars)
            float barW = r * 0.25f;
            float barH = r * 0.7f;
            float gap  = r * 0.2f;
            ImU32 col  = IM_COL32(255, 60, 60, 255); // Red for Pause action available
            dl->AddRectFilled(ImVec2(center.x - gap - barW, center.y - barH * 0.5f), ImVec2(center.x - gap, center.y + barH * 0.5f), col, 4.0f);
            dl->AddRectFilled(ImVec2(center.x + gap, center.y - barH * 0.5f), ImVec2(center.x + gap + barW, center.y + barH * 0.5f), col, 4.0f);
        } else {
            // Play Symbol (Triangle)
            float triR = r * 0.45f;
            ImVec2 p1(center.x - triR * 0.6f, center.y - triR);
            ImVec2 p2(center.x - triR * 0.6f, center.y + triR);
            ImVec2 p3(center.x + triR, center.y);
            dl->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 255, 100, 255)); // Green for Play
        }

        // Vertical-only drag
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - windowHeight);
        }

        if (clicked) {
            if (cancelMode) {
                persistent_bool[O("bAutoQueue")] = false;
                g_aqCounting = false;
            } else {
                AutoPlay::bAutoPlaying = !AutoPlay::bAutoPlaying;
                if (AutoPlay::bAutoPlaying) {
                    // Set mode based on priority
                    if (AutoPlay::bAutoPlaySwitch) {
                        AutoPlay::currentMode = AutoPlay::MODE_AUTO_PLAY;
                    } else if (AutoPlay::bAutoAimSwitch) {
                        AutoPlay::currentMode = AutoPlay::MODE_AUTO_AIM;
                    }
                    AutoPlay::ClearState();
                } else {
                    AutoPlay::currentMode = AutoPlay::MODE_OFF;
                }
            }
        }
    }
    End();
    PopStyleVar();
    PopStyleColor(2);
}

static void DrawFloatingButton(ImGuiIO& io) {
    static GLuint logo_tex   = LoadTextureFromMemory(logo_png, logo_png_len);
    static bool   isDragging = false;

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    float buttonRadius = 70.0f * scale; // Total size 140px
    float buttonSize   = buttonRadius * 2.0f;
    float winSize      = buttonSize + 15.0f * scale; // 155px
    float margin       = 12.0f * scale;

    float toggleWinH = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.y * 2.0f;
    const float rightMargin = 20.0f * scale;
    float toggleWinW = GetFrameHeight() * 1.7f + GetStyle().WindowPadding.x * 2.0f;
    float fixedX = io.DisplaySize.x - rightMargin - ImMax(winSize, toggleWinW);

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 80.0f - toggleWinH;

    float posY = g_sideBtnsY - winSize - margin;

    SetNextWindowPos(ImVec2(fixedX, posY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(winSize, winSize), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl     = GetWindowDrawList();
        ImVec2      center = ImVec2(fixedX + buttonRadius + 5, posY + buttonRadius + 5);

        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));

        // Circular Background
        dl->AddCircleFilled(center, buttonRadius, IM_COL32(15, 15, 20, 255));
        
        // Animated Pulse Glow (Only when AutoPlay is ON)
        static float pulsePhase = 0.0f;
        if (AutoPlay::bAutoPlaying) {
            pulsePhase += io.DeltaTime * 3.5f;
            float pulseAlpha = (sinf(pulsePhase) * 0.5f + 0.5f); // 0.0 to 1.0
            
            dl->AddCircle(center, buttonRadius + 2.0f + (pulseAlpha * 3.0f), IM_COL32(255, 120, 0, (int)(100 * (1.0f - pulseAlpha))), 0, 4.0f);
            dl->AddCircle(center, buttonRadius + 4.0f + (pulseAlpha * 6.0f), IM_COL32(255, 120, 0, (int)(50 * (1.0f - pulseAlpha))), 0, 8.0f);
        }

        // Rotating Segmented Border (Outer)
        static float rotAngle = 0.0f;
        if (AutoPlay::bAutoPlaying) {
            rotAngle += io.DeltaTime * 1.5f;
        }
        
        int num_segments = 6;
        float dash_length = (2.0f * 3.141592654f) / (num_segments * 2.0f);
        for (int i = 0; i < num_segments; i++) {
            float a_min = rotAngle + i * dash_length * 2.0f;
            float a_max = a_min + dash_length;
            // Move it slightly outward (buttonRadius + 6.0f) to avoid overlapping the main solid border
            dl->PathArcTo(center, buttonRadius + 6.0f, a_min, a_max, 10);
            // Increased thickness to 6.0f and opacity to 255 for better visibility
            dl->PathStroke(IM_COL32(255, 120, 0, 255), 0, 6.0f); 
        }

        // Main Solid Border (Slightly thicker as requested)
        dl->AddCircle(center, buttonRadius, IM_COL32(255, 120, 0, 255), 0, 5.5f);
        
        // Inner Fine Ring
        dl->AddCircle(center, buttonRadius - 5.0f, IM_COL32(255, 120, 0, 150), 0, 2.0f);
        
        // Draw logo — Perfectly Rounded
        ImVec2 p_min(center.x - buttonRadius + 8.0f, center.y - buttonRadius + 8.0f);
        ImVec2 p_max(center.x + buttonRadius - 8.0f, center.y + buttonRadius - 8.0f);
        dl->AddImageRounded((void*)(intptr_t)logo_tex, p_min, p_max, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, buttonRadius);

        // Vertical-only drag moves both buttons together via g_sideBtnsY
        if (IsItemActive() && IsMouseDragging(0)) {
            isDragging = true;
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, winSize + margin,
                                  io.DisplaySize.y - 80.0f - toggleWinH);
        }

        if (IsItemHovered() && IsMouseReleased(0) && !isDragging)
            g_menu.isOpen = !g_menu.isOpen;

        if (!IsItemActive()) isDragging = false;
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}


INLINE void DrawUpdateRequired(ImGuiIO& io) {
    // Full screen black transparent overlay
    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 0.92f));
    Begin(O("##OverlayUpdate"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = 860;
    float cardH = (io.DisplaySize.y < 850) ? io.DisplaySize.y * 0.95f : 820;

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 30.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##UpdateCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    // Draw Orange Border (Stylish bold look)
    dl->AddRect(winPos, winPos + ImVec2(cardW, cardH), IM_COL32(255, 127, 0, 200), 30.0f, 0, 13.0f);
    dl->AddRect(winPos + ImVec2(2, 2), winPos + ImVec2(cardW - 2, cardH - 2), IM_COL32(255, 127, 0, 40), 30.0f, 0, 1.0f);

    // Title Section: "UPDATE REQUIRED" (Orange bold text - moved higher)
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    SetCursorPosY(cardH * 0.085f);
    SetWindowFontScale(1.7f);
    ImVec2 titleSize = CalcTextSize(O("UPDATE REQUIRED"));
    SetCursorPosX((cardW - titleSize.x) * 0.5f);
    
    TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), O("UPDATE REQUIRED"));
    SetWindowFontScale(1.0f);
    if (g_ArialBlackFont) PopFont();

    // Text Description (centered, multi-line, with "APK" in yellow and written "chota")
    std::string p1 = O("This version is outdated for the new ");
    std::string p2 = O("APK");
    std::string p3 = O(" file");
    std::string line2Text = O("please click the button below and download");
    std::string enjoyText = O("Enjoy LionX");

    float w1 = CalcTextSize(p1.c_str()).x;
    float w2 = CalcTextSize(p2.c_str()).x;
    float w3 = CalcTextSize(p3.c_str()).x;
    float totalW1 = w1 + w2 + w3;
    float totalW2 = CalcTextSize(line2Text.c_str()).x;
    float totalW3 = CalcTextSize(enjoyText.c_str()).x;

    // Line 1: Centered with Yellow APK
    SetCursorPosY(cardH * 0.21f);
    SetWindowFontScale(0.95f);
    SetCursorPosX((cardW - totalW1) * 0.5f);
    TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s", p1.c_str());
    SameLine(0, 0);
    TextColored(ImVec4(1.0f, 0.9f, 0.0f, 1.0f), "%s", p2.c_str()); // Yellow
    SameLine(0, 0);
    TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s", p3.c_str());

    // Line 2: Centered white text below Line 1
    SetCursorPosY(cardH * 0.21f + 30.0f);
    SetCursorPosX((cardW - totalW2) * 0.5f);
    TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s", line2Text.c_str());

    // Line 3: Very small orange "Enjoy LionX" below Line 2
    SetCursorPosY(cardH * 0.21f + 65.0f);
    SetWindowFontScale(0.80f); // Very small
    SetCursorPosX((cardW - totalW3) * 0.5f);
    TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "%s", enjoyText.c_str()); // Orange

    SetWindowFontScale(1.0f);

    // Download Button
    SetCursorPosY(cardH * 0.51f);
    SetCursorPosX(70);
    PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.55f, 0.1f, 1.0f));
    PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.35f, 0.0f, 1.0f));
    PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    
    if (Button(O("##DownloadBtn"), ImVec2(cardW - 140, cardH * 0.17f))) {
        if (!g_UpdateURL.empty()) {
            OpenURL(g_UpdateURL.c_str());
        }
    }

    // Draw text and icon on top of the button
    ImVec2 btnPos = GetItemRectMin();
    ImVec2 btnSize = GetItemRectSize();
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    SetWindowFontScale(1.4f);
    ImVec2 btnTextSize = CalcTextSize(O("Download Now"));
    dl->AddText(ImVec2(btnPos.x + (btnSize.x - btnTextSize.x) * 0.5f + 20, btnPos.y + (btnSize.y - btnTextSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), O("Download Now"));
    SetWindowFontScale(1.0f);
    if (g_ArialBlackFont) PopFont();

    // Draw Download Icon using FontAwesome to the left of the text
    if (g_IconFont) {
        PushFont(g_IconFont);
        SetWindowFontScale(1.4f); // Match the scale of download button text
        ImVec2 downloadIconSize = CalcTextSize("\uF019");
        float textX = btnPos.x + (btnSize.x - btnTextSize.x) * 0.5f + 20.0f;
        ImVec2 iconDrawPos = ImVec2(
            textX - downloadIconSize.x - 15.0f,
            btnPos.y + (btnSize.y - downloadIconSize.y) * 0.5f
        );
        dl->AddText(iconDrawPos, IM_COL32(255, 255, 255, 255), "\uF019");
        SetWindowFontScale(1.0f);
        PopFont();
    }

    PopStyleVar();
    PopStyleColor(3);

    // Community Footer (Two Identical Telegram Icons - Texture Based)
    SetCursorPosY(cardH * 0.73f);
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    ImVec2 commSize = CalcTextSize(O("Join Community"));
    SetCursorPosX((cardW - commSize.x) * 0.5f);
    TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("Join Community"));
    if (g_ArialBlackFont) PopFont();
    
    static GLuint tele_tex = 0;
    if (tele_tex == 0) {
        tele_tex = LoadTextureFromMemory(tele_logo_img, sizeof(tele_logo_img));
    }

    float iconSide = cardH * 0.134f; // Upscaled
    float iconY = cardH * 0.88f;

    auto DrawTeleLogo = [&](float centerX, const char* url) {
        ImVec2 center = winPos + ImVec2(centerX, iconY);
        ImVec2 topLeft = center - ImVec2(iconSide * 0.5f, iconSide * 0.5f);
        ImVec2 bottomRight = center + ImVec2(iconSide * 0.5f, iconSide * 0.5f);
        
        if (tele_tex) {
            dl->AddImageRounded((ImTextureID)(intptr_t)tele_tex, topLeft, bottomRight, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, 15.0f);
        }
        
        SetCursorScreenPos(topLeft);
        if (InvisibleButton((O("##TeleBtnUpdate_") + std::to_string((int)centerX)).c_str(), ImVec2(iconSide, iconSide))) {
            OpenURL(url);
        }
    };

    DrawTeleLogo(cardW * 0.35f, O("https://t.me/+u-pJg-9rRxszY2Y1")); // Left Icon
    DrawTeleLogo(cardW * 0.65f, O("https://t.me/Lion_X_Engine")); // Right Icon

    End(); // UpdateCard
    PopStyleVar(3);
    PopStyleColor();

    End(); // OverlayUpdate
}


static bool first_time = true;
INLINE void DrawLogin(ImGuiIO& io) {
    if ((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0) return DrawMenu(io);

    // Full screen overlay
    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 0.92f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = 860;
    float cardH = (io.DisplaySize.y < 850) ? io.DisplaySize.y * 0.95f : 820;

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 30.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    // Draw Orange Border (Stylish bold look)
    dl->AddRect(winPos, winPos + ImVec2(cardW, cardH), IM_COL32(255, 127, 0, 200), 30.0f, 0, 13.0f);
    dl->AddRect(winPos + ImVec2(2, 2), winPos + ImVec2(cardW - 2, cardH - 2), IM_COL32(255, 127, 0, 40), 30.0f, 0, 1.0f);

    // Title Section
    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    SetCursorPosY(cardH * 0.085f);
    SetWindowFontScale(1.7f);
    ImVec2 titleSize = CalcTextSize(O("LionX v1.0.1"));
    SetCursorPosX((cardW - titleSize.x) * 0.5f);
    
    TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), O("LionX v1.0.1"));
    
    SetWindowFontScale(1.0f);
    if (g_ArialBlackFont) PopFont();



    // --- SECURITY HEARTBEAT WATCHDOG ---
    static uint64_t lastHeartbeat = 0;
    static time_t lastHeartbeatTime = 0;
    if (lastHeartbeatTime == 0) {
        lastHeartbeat = Security::securityHeartbeat;
        lastHeartbeatTime = time(nullptr);
    } else {
        if (Security::securityHeartbeat != lastHeartbeat) {
            lastHeartbeat = Security::securityHeartbeat;
            lastHeartbeatTime = time(nullptr);
        } else if (time(nullptr) - lastHeartbeatTime > 30) {
            // Security thread was killed or frozen!
            exit(0);
        }
    }
    
    if (is_logging_in) {
        SetCursorPosY(cardH * 0.45f);
        static float spinnerAngle = 0.0f;
        spinnerAngle += io.DeltaTime * 6.0f;
        ImVec2 center = winPos + ImVec2(cardW * 0.5f, cardH * 0.5f);
        for (int i = 0; i < 8; i++) {
            float a = spinnerAngle + (i * 3.1415f * 2.0f / 8.0f);
            dl->AddCircleFilled(center + ImVec2(cosf(a), sinf(a)) * 50.0f, 9.0f, IM_COL32(255, 127, 0, (int)(255 * (8 - i) / 8.0f)));
        }
        SetCursorPosY(cardH * 0.70f);
        ImVec2 authTextSize = CalcTextSize(O("Authenticating..."));
        SetCursorPosX((cardW - authTextSize.x) * 0.5f);
        TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), O("Authenticating..."));
    } else {
        // Error Message Display (Small text above input field)
        if (ERROR_CODE != 0 || !ERROR_MESSAGE.empty()) {
            std::string displayError = ERROR_MESSAGE.empty() ? O("Connection Error!") : ERROR_MESSAGE;
            if (ERROR_CODE == 0xE05) displayError = O("Server busy, try again.");
            if (ERROR_CODE == 0xE10) displayError = O("Invalid License Key!");
            if (ERROR_CODE == 0xE11) displayError = O("Data sync error.");
            if (ERROR_CODE == 0xE15) displayError = O("Key used on another device!");
            if (ERROR_CODE == 0xE20) displayError = O("License Expired!");
            
            SetCursorPosY(cardH * 0.21f);
            SetWindowFontScale(0.85f);
            ImVec2 errSize = CalcTextSize(displayError.c_str());
            SetCursorPosX((cardW - errSize.x) * 0.5f);
            TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", displayError.c_str());
            SetWindowFontScale(1.0f);
        }

        // Key Input Display Area
        SetCursorPosY(cardH * 0.26f);
        ImVec2 inputSize = ImVec2(cardW - 140, cardH * 0.17f);
        SetCursorPosX(70);
        ImVec2 inputPos = GetCursorScreenPos();
        
        // Input Background with rounded corners
        dl->AddRectFilled(inputPos, inputPos + inputSize, IM_COL32(25, 25, 30, 255), 20.0f);
        dl->AddRect(inputPos, inputPos + inputSize, IM_COL32(255, 127, 0, 40), 20.0f, 0, 2.0f);
        
        // Key Text or Placeholder (Centered vertically)
        SetCursorPosY(cardH * 0.26f + (inputSize.y - 45) / 2.0f); 
        SetCursorPosX(100);
        static std::string loginError = "";
        std::string displayKey = persistent_string["key"];
        bool isPlaceholder = displayKey.empty();
        if (isPlaceholder) displayKey = O("XXXXX-XXXXX-XXXXX-XXXXX-XXXXX");
        
        if (isPlaceholder) {
            SetWindowFontScale(1.15f); // Scale text down so placeholder doesn't overlap
            TextColored(ImVec4(0.45f, 0.45f, 0.5f, 1.0f), "%s", displayKey.c_str()); // Lighter color ("halka")
        } else {
            SetWindowFontScale(1.4f); // Larger scale for actual key (23 chars) so it fills the space nicely
            TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "%s", displayKey.c_str()); // Bright color for real key
        }
        SetWindowFontScale(1.0f);
        
        // Clipboard Icon: FontAwesome Vector Icon (Orange)
        float iconSize = inputSize.y * 0.46f; // Kept size the same for the InvisibleButton click area
        ImVec2 iconPos = inputPos + ImVec2(inputSize.x - iconSize - 20, (inputSize.y - iconSize) / 2.0f);
        
        if (g_IconFont) {
            PushFont(g_IconFont);
            SetWindowFontScale(1.4f); // Scale it beautifully to fit inside the text box
            ImVec2 faClipboardSize = CalcTextSize("\uF46D");
            // Center the FontAwesome icon inside the click bounds
            ImVec2 iconDrawPos = ImVec2(
                iconPos.x + (iconSize - faClipboardSize.x) * 0.5f,
                iconPos.y + (iconSize - faClipboardSize.y) * 0.5f
            );
            dl->AddText(iconDrawPos, IM_COL32(255, 127, 0, 255), "\uF46D");
            SetWindowFontScale(1.0f);
            PopFont();
        }

        // Invisible Button over Icon for functional Paste
        SetCursorScreenPos(iconPos);
        if (InvisibleButton(O("##PasteBtn"), ImVec2(iconSize, iconSize))) {
            JNIEnv* env;
            jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (getEnvResult == JNI_EDETACHED) VM->AttachCurrentThread(&env, nullptr);
            
            std::string pasted = getClipboard(env);
            if (!pasted.empty()) {
                pasted.erase(pasted.find_last_not_of(" \n\r\t") + 1); // Clean up extra spaces
                persistent_string["key"] = pasted;
                loginError = ""; // Clear error on new paste
                ERROR_MESSAGE = ""; // Also clear the global error so wrong format goes away
                save_persistence();
            }
        }

        // Authenticate Button
        SetCursorPosY(cardH * 0.51f);
        SetCursorPosX(70);
        PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.45f, 0.0f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.55f, 0.1f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.35f, 0.0f, 1.0f));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        
        if (Button(O("##AuthBtn"), ImVec2(cardW - 140, cardH * 0.17f))) {
            JNIEnv* env;
            jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (getEnvResult == JNI_EDETACHED) VM->AttachCurrentThread(&env, nullptr);
            
            std::string keyToUse = persistent_string["key"];
            if (keyToUse.empty()) keyToUse = getClipboard(env);
            
            // 7-DAY TIMED BYPASS KEY
            if (keyToUse == O("LION-56BF-8628-FFNK-9A2D")) {
                loginError = "";
                g_AuthToken = (uint64_t)g_ExpiryTime ^ 0xDEADBEEFCAFEBABE;
                g_Token = O("LIONX_VIP_7D");
                g_Auth = O("LIONX_VIP_7D");
                
                // Persistence Logic for 7-Day tenure
                if (persistent_string[O("bypass_act_ts")].empty()) {
                    persistent_string[O("bypass_act_ts")] = std::to_string((int64_t)time(nullptr));
                    save_persistence();
                }
                
                int64_t actTs = std::stoll(persistent_string[O("bypass_act_ts")]);
                G_EXPIRY_TS = actTs + (7LL * 24LL * 3600LL);
                g_ExpTime = formatTimestamp(G_EXPIRY_TS);
                
                g_menu.isOpen = true;
                first_time = false;
            } else if (!DEBUG_BYPASS_LOGIN) {
                // Real Login Logic - Format validation
                bool isFormatValid = false;
                if (keyToUse.length() == 23) {
                    isFormatValid = true;
                    for (int i = 0; i < 23; i++) {
                        if (i == 5 || i == 11 || i == 17) {
                            if (keyToUse[i] != '-') { isFormatValid = false; break; }
                        } else {
                            if (!isalnum(keyToUse[i])) { isFormatValid = false; break; }
                        }
                    }
                }

                if (!isFormatValid) {
                    // Clear the invalid key from persistence so auto-login won't hit the server
                    persistent_string["key"] = "";
                    save_persistence();
                    
                    // Fake a 2-second authentication process
                    std::thread([]() {
                        is_logging_in = true;
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        is_logging_in = false;
                        ERROR_MESSAGE = O("Wrong Format");
                        ERROR_CODE = 0;
                    }).detach();
                } else {
                    ERROR_MESSAGE = "";
                    std::thread([keyToUse]() {
                        Login(getAndroidID(nullptr), keyToUse);
                    }).detach();
                }
            } else {
                loginError = "";
                g_AuthToken = (uint64_t)g_ExpiryTime ^ 0xDEADBEEFCAFEBABE; 
                g_Token = O("DEBUG_BYPASS");
                g_Auth = O("DEBUG_BYPASS");
                g_menu.isOpen = true;
                first_time = false;
            }
        }
        
        // Draw text and icon on top of the button
        ImVec2 btnPos = GetItemRectMin();
        ImVec2 btnSize = GetItemRectSize();
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(1.4f);
        ImVec2 authTextSize = CalcTextSize(O("AUTHENTICATE"));
        dl->AddText(ImVec2(btnPos.x + (btnSize.x - authTextSize.x) * 0.5f + 35, btnPos.y + (btnSize.y - authTextSize.y) * 0.5f), IM_COL32(255, 255, 255, 255), O("AUTHENTICATE"));
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();
        
        // Login Icon on button: Stylish FontAwesome Key Icon
        if (g_IconFont) {
            PushFont(g_IconFont);
            SetWindowFontScale(1.0f); // Sleek standard scale for key icon (thinner and smaller)
            ImVec2 keyIconSize = CalcTextSize("\uF084");
            float textX = btnPos.x + (btnSize.x - authTextSize.x) * 0.5f + 35.0f;
            ImVec2 iconDrawPos = ImVec2(
                textX - keyIconSize.x - 18.0f,
                btnPos.y + (btnSize.y - keyIconSize.y) * 0.5f
            );
            dl->AddText(iconDrawPos, IM_COL32(255, 255, 255, 255), "\uF084");
            SetWindowFontScale(1.0f);
            PopFont();
        }

        PopStyleVar();
        PopStyleColor(3);

        // Community Footer (Two Identical Orange Logos - Texture Based)
        SetCursorPosY(cardH * 0.73f);
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        ImVec2 commSize = CalcTextSize(O("Join Community"));
        SetCursorPosX((cardW - commSize.x) * 0.5f);
        TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), O("Join Community"));
        if (g_ArialBlackFont) PopFont();
        
        static GLuint tele_tex = 0;
        if (tele_tex == 0) {
            tele_tex = LoadTextureFromMemory(tele_logo_img, sizeof(tele_logo_img));
        }

        float iconSide = cardH * 0.134f; // Upscaled as requested
        float iconY = cardH * 0.88f;

        auto DrawTeleLogo = [&](float centerX, const char* url) {
            ImVec2 center = winPos + ImVec2(centerX, iconY);
            ImVec2 topLeft = center - ImVec2(iconSide * 0.5f, iconSide * 0.5f);
            ImVec2 bottomRight = center + ImVec2(iconSide * 0.5f, iconSide * 0.5f);
            
            // Draw exact logo from provided image with rounded corners
            if (tele_tex) {
                dl->AddImageRounded((ImTextureID)(intptr_t)tele_tex, topLeft, bottomRight, ImVec2(0,0), ImVec2(1,1), IM_COL32_WHITE, 15.0f);
            }
            
            // Click area
            SetCursorScreenPos(topLeft);
            if (InvisibleButton((O("##TeleBtn_") + std::to_string((int)centerX)).c_str(), ImVec2(iconSide, iconSide))) {
                OpenURL(url);
            }
        };

        DrawTeleLogo(cardW * 0.35f, O("https://t.me/+u-pJg-9rRxszY2Y1")); // Left Icon
        DrawTeleLogo(cardW * 0.65f, O("https://t.me/Lion_X_Engine")); // Right Icon
    }

    End(); // LoginCard
    PopStyleVar(3);
    PopStyleColor();

    End(); // Overlay
}



INLINE void SetupImgui() {
    CheckAppUpdate();
    PACKAGE_NAME = string(getcmdline());

    ImGui::CreateContext();

    auto& style = ImGui::GetStyle();
    auto& io = ImGui::GetIO();

    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;

    switch_theme(current_theme);

    load_persistence();
    
    // --- AUTO LOGIN LOGIC ---
    std::string savedKey = persistent_string["key"];
    if (!savedKey.empty() && !((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0)) {
        std::thread([savedKey]() {
            // Give system some time to stabilize
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            Login(getAndroidID(nullptr), savedKey);
        }).detach();
    }

    AutoPlay::bAutoPlaySwitch = persistent_bool[O("bAutoPlaySwitch")];
    AutoPlay::bAutoAimSwitch = persistent_bool[O("bAutoAimSwitch")];
    svConfig_Load();
    load_imgui_style();

    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    ImFontConfig font_cfg;
    font_cfg.SizePixels = persistent_float["fFontScale"];
    font_cfg.FontDataOwnedByAtlas = false;
    g_SegoeUIFont = io.Fonts->AddFontFromMemoryTTF((void*)segoeui_ttf, segoeui_ttf_len, persistent_float["fFontScale"], &font_cfg);
    g_ArialBlackFont = io.Fonts->AddFontFromMemoryTTF((void*)arial_black_ttf, arial_black_ttf_len, persistent_float["fFontScale"], &font_cfg);

    static const ImWchar icon_ranges[] = {
        0xF013, 0xF013, // Gear/Cog (Extra)
        0xF019, 0xF019, // Download (Update)
        0xF05A, 0xF05A, // Info-circle (Info)
        0xF084, 0xF084, // Key (Login)
        0xF1DA, 0xF1DA, // History/Clock (Queue)
        0xF46D, 0xF46D, // Clipboard List (Paste)
        0xF4D7, 0xF4D7, // Route/Path (Draw)
        0xF544, 0xF544, // Robot (Play)
        0
    };
    ImFontConfig icon_cfg;
    icon_cfg.FontDataOwnedByAtlas = false;
    float iconScale = (persistent_float["fFontScale"] > 5.0f) ? (persistent_float["fFontScale"] / 22.0f) : 1.0f;
    g_IconFont = io.Fonts->AddFontFromMemoryTTF((void*)font_awesome_ttf, font_awesome_ttf_len, 35.0f * iconScale, &icon_cfg, icon_ranges);

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));

    bImguiSetup = true;
}

INLINE void DrawResolutionText(ImGuiIO& io, int w, int h) {
    if (persistent_bool[O("bHideResText")]) return;

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    SetNextWindowPos(ImVec2(30.0f * scale, 105.0f * scale));

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ResTextWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs)) {
        
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(0.95f * scale);
        
        char resBuf[64];
        snprintf(resBuf, sizeof(resBuf), "Resolution: %d x %d", w, h);
        
        ImVec2 pos = GetCursorScreenPos();
        ImDrawList* dl = GetWindowDrawList();
        
        ImU32 outCol = IM_COL32(0, 0, 0, 255);
        ImU32 textCol = IM_COL32(0, 255, 0, 255);
        
        // Sharp outline for that professional overlay look
        dl->AddText(ImVec2(pos.x - 1.0f, pos.y), outCol, resBuf);
        dl->AddText(ImVec2(pos.x + 1.0f, pos.y), outCol, resBuf);
        dl->AddText(ImVec2(pos.x, pos.y - 1.0f), outCol, resBuf);
        dl->AddText(ImVec2(pos.x, pos.y + 1.0f), outCol, resBuf);
        
        // Main green text
        dl->AddText(pos, textCol, resBuf);

        ImVec2 ts = CalcTextSize(resBuf);
        SetWindowSize(ImVec2(ts.x + 10, ts.y + 10));
        
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

INLINE void DrawFPSCounter(ImGuiIO& io) {
    if (!persistent_bool[O("bFpsCounter")]) return;

    static float g_fpsCounterY = 0.0f;
    if (g_fpsCounterY == 0.0f) {
        g_fpsCounterY = Height * 0.4f; // Default starting height
    }

    float scale = ImClamp(io.DisplaySize.y / 1080.0f, 0.65f, 1.0f);
    float button_size = 130.0f * scale; // Same as play/pause button
    float radius = button_size * 0.5f;
    
    // Fixed X pos, Draggable Y pos
    float margin = 20.0f * scale;
    float fixedX = margin; 

    float windowWidth = button_size + 20.0f * scale;
    float windowHeight = button_size + 20.0f * scale;

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(fixedX, g_fpsCounterY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FPSCounterWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImVec2 pos = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
        
        // Handle dragging
        bool clicked = InvisibleButton(O("##FPSHit"), size);
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_fpsCounterY += io.MouseDelta.y;
            g_fpsCounterY = ImClamp(g_fpsCounterY, 0.0f, io.DisplaySize.y - windowHeight);
        }

        ImDrawList* dl = GetWindowDrawList();
        
        // Stylish Background (Matching Play/Pause button style)
        dl->AddCircleFilled(center, radius, IM_COL32(18, 18, 22, 255));
        
        // FPS logic with Smooth Interpolation (EMA)
        static float smoothedFps = 60.0f;
        if (smoothedFps == 60.0f && io.Framerate > 0.0f) {
            smoothedFps = io.Framerate;
        }
        smoothedFps += (io.Framerate - smoothedFps) * 0.07f; // Smooth transition
        
        char fpsBuf[16];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f", smoothedFps);
        
        ImU32 fpsCol = IM_COL32(0, 255, 100, 255); // Vibrant Green
        if (smoothedFps < 30.0f) {
            fpsCol = IM_COL32(255, 50, 50, 255);   // Red
        } else if (smoothedFps < 45.0f) {
            fpsCol = IM_COL32(255, 120, 0, 255);   // Orange
        } else if (smoothedFps < 55.0f) {
            fpsCol = IM_COL32(255, 215, 0, 255);   // Golden Yellow
        }

        // --- DYNAMIC ORANGE & YELLOW HUD CIRCLE ---
        // 1. Dark grey track background
        dl->AddCircle(center, radius, IM_COL32(45, 45, 50, 255), 0, 4.0f);
        
        // 2. Outer orange glowing ring
        dl->AddCircle(center, radius + 2.0f, IM_COL32(255, 120, 0, 80), 0, 2.0f);
        
        // 3. Dynamic Golden Yellow Progress Arc based on FPS
        float angle_max = (smoothedFps / 60.0f) * 2.0f * IM_PI;
        if (angle_max > 2.0f * IM_PI) angle_max = 2.0f * IM_PI;
        if (angle_max < 0.0f) angle_max = 0.0f;
        
        dl->PathArcTo(center, radius, -IM_PI * 0.5f, -IM_PI * 0.5f + angle_max, 40);
        dl->PathStroke(IM_COL32(255, 215, 0, 255), 0, 4.0f);

        // Render FPS value
        SetWindowFontScale(2.5f * scale);
        ImVec2 ts = CalcTextSize(fpsBuf);
        dl->AddText(ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f - 8.0f * scale), fpsCol, fpsBuf);
        
        // Render Label
        SetWindowFontScale(1.0f * scale);
        ImVec2 tsLbl = CalcTextSize("FPS");
        dl->AddText(ImVec2(center.x - tsLbl.x * 0.5f, center.y + ts.y * 0.5f - 10.0f * scale), IM_COL32(200, 200, 220, 255), "FPS");
        
        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {

    eglQuerySurface(dpy, surface, EGL_WIDTH, &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);

    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);

    screenCenter = Vector2(Width / 2, Height / 2);

    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();
    if (g_UpdateRequired) {
        DrawUpdateRequired(io);
        static bool lastState = true;
        bool currentState = true;
        if (currentState != lastState) {
            UpdateOverlayFlags(currentState);
            lastState = currentState;
        }
    } else if (IsExpired()) {
        DrawExpired(io);
    } else if (((g_AuthToken ^ 0xDEADBEEFCAFEBABE) == g_ExpiryTime && g_ExpiryTime > 0) || DEBUG_BYPASS_LOGIN || (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth)) {
        DrawFloatingButton(io);
        DrawMenu(io);
        DrawToggleButton(false);
        DrawFPSCounter(io);
        DrawResolutionText(io, Width, Height);
        
        // Update Touch Mask: If menu is closed and no modal is active, make window transparent to touches
        static bool lastState = true;
        bool currentState = g_menu.isOpen || g_aqCounting || first_time;
        if (currentState != lastState) {
            UpdateOverlayFlags(currentState);
            lastState = currentState;
        }
    } else {
        DrawLogin(io);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui_ClearHoverEffect();

    return _Draw(dpy, surface);
}

void __IMGUI__() {
    create_directory_recursive(CONC(O("/data/user_de/0/"), PACKAGE_NAME.c_str(), O("/no_backup")));
}
