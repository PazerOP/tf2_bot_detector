/*
  Created on 07.07.18.
*/

#pragma once

#include <glez/font.hpp>
#include <glez/color.hpp>

namespace zerokernel
{

class TextComponent
{
public:
    TextComponent(fonts::font &font);

    void render(const std::string &string, rgba_t color, rgba_t outline);
    // Uses default colors
    void render(const std::string &string);

    void move(int x, int y);

    int x{};
    int y{};
    int padding_x{};
    int padding_y{};
    int size_x{};
    int size_y{};

    fonts::font &font;
    const rgba_t &default_text;
    const rgba_t &default_outline;
};
} // namespace zerokernel