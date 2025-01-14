#include "StyleRepository.h"

#include <stdexcept>

#include <imgui.h>
#include <imgui_freetype.h>

#include "Fonts/DroidSans.h"
#include "Fonts/Cousine-Regular.h"
#include "IconFontCppHeaders/FontAwesomeSolid.h"
#include "IconFontCppHeaders/IconsFontAwesome5.h"

#include "Resources.h"

_StyleRepository::_StyleRepository()
{
    ImGuiIO& io = ImGui::GetIO();

    if (io.Fonts->AddFontFromMemoryCompressedTTF(
            DroidSans_compressed_data, DroidSans_compressed_size, 16.0f)
        == NULL) {
        throw std::runtime_error("Could not load font.");
    };

    ImFontConfig configMerge;
    configMerge.MergeMode = true;
    configMerge.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_LightHinting;
    static const ImWchar rangesIcons[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    io.Fonts->AddFontFromMemoryCompressedTTF(
        FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 16.0f, &configMerge, rangesIcons);

    _mediumFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        DroidSans_compressed_data, DroidSans_compressed_size, 24.0f);
    if (_mediumFont == NULL) {
        throw std::runtime_error("Could not load font.");
    }
    _largeFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        DroidSans_compressed_data, DroidSans_compressed_size, 48.0f);
    if (_largeFont == NULL) {
        throw std::runtime_error("Could not load font.");
    }
    _monospaceFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        Cousine_Regular_compressed_data, Cousine_Regular_compressed_size, 14.0f);
    if (_monospaceFont == NULL) {
        throw std::runtime_error("Could not load font.");
    }
}

ImFont* _StyleRepository::getMediumFont() const
{
    return _mediumFont;
}

ImFont* _StyleRepository::getLargeFont() const
{
    return _largeFont;
}

ImFont* _StyleRepository::getMonospaceFont() const
{
    return _monospaceFont;
}
