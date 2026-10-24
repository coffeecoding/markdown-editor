#pragma once
#include <windows.h>

struct ThemePalette {
    COLORREF background, text, link, muted, code;
};

inline ThemePalette Palette(bool dark)
{
    return dark ? ThemePalette{RGB(0, 0, 0), RGB(208, 208, 208), RGB(121, 184, 255),
                              RGB(160, 160, 160), RGB(24, 24, 24)}
                : ThemePalette{RGB(245, 243, 238), RGB(36, 36, 36), RGB(0, 91, 174),
                              RGB(98, 96, 92), RGB(232, 229, 222)};
}
