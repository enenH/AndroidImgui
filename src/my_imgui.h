#pragma once

#include "imgui.h"

namespace ImGui {
    bool Android_LoadSystemFont(float SizePixels);

    const ImWchar *GetGlyphRangesChineseTraditionalOfficial();

    const ImWchar *GetGlyphRangesChineseSimplifiedOfficial();
}