/*
** colorspace.h
**
** Convert between colorspaces
**
**---------------------------------------------------------------------------
**
** Copyright 2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
*/

#pragma once

#include <tuple>
#include <cstdint>
#include <string_view>

constexpr inline int Luminance(int r, int g, int b)
{
	return (r * 77 + g * 143 + b * 37) >> 8;
}

namespace Color {

consteval uint32_t str(std::string_view s, bool opaque = false)
{
	if ((s.length() != 7 && s.length() != 4 && s.length() != 9 && s.length() != 5) || s[0] != '#') throw "Not a color";
	s = s.substr(1);
	uint32_t rgb = opaque? 0xff: 0;
	for (int i = 0, b = 0; i < 8 && b < s.length(); i++, b = i/(8/s.length())) {
		rgb <<= 4;
		if (s[b] >= '0' && s[b] <= '9') rgb |= (s[b] - '0');
		else if (s[b] >= 'a' && s[b] <= 'f') rgb |= (s[b] - 'a' + 10);
		else if (s[b] >= 'A' && s[b] <= 'F') rgb |= (s[b] - 'A' + 10);
		else throw "Bad byte";
	}
	return rgb;
}

#define T(a, b, c) \
	static_assert(Color::str(a) == Color::str(b), ""); \
	static_assert(Color::str(a) == c, "");
T("#000", "#000000", 0x000000);
T("#f00", "#ff0000", 0xff0000);
T("#0f0", "#00ff00", 0x00ff00);
T("#00f", "#0000ff", 0x0000ff);
T("#ff0", "#ffff00", 0xffff00);
T("#0ff", "#00ffff", 0x00ffff);
T("#f0f", "#ff00ff", 0xff00ff);
T("#fff", "#ffffff", 0xffffff);
static_assert(str("#7f7f7f") == 0x7f7f7f, "");
static_assert(str("#7f7f7f", true) == 0xff7f7f7f, "");
static_assert(str("#007f7f7f") == 0x007f7f7f, "");
static_assert(str("#7f7f7f7f") == 0x7f7f7f7f, "");
static_assert(str("#ff7f7f7f") == 0xff7f7f7f, "");
#undef T

template <char C>
consteval int channel(std::string_view s)
{
	auto b = [&s](int c) consteval { return 0xff & ( str(s)>>c ); };
	if (C == 'a' || C == 'A') return b(24);
	if (C == 'r' || C == 'R') return b(16);
	if (C == 'g' || C == 'G') return b(8);
	if (C == 'b' || C == 'B') return b(0);
	throw "Bad channel";
}

#define COLOR_SPREAD__1(rgb, a)          Color::channel<a>(rgb)
#define COLOR_SPREAD__2(rgb, a, b)       Color::channel<a>(rgb),COLOR_SPREAD__1(rgb, b)
#define COLOR_SPREAD__3(rgb, a, b, c)    Color::channel<a>(rgb),COLOR_SPREAD__2(rgb, b, c)
#define COLOR_SPREAD__4(rgb, a, b, c, d) Color::channel<a>(rgb),COLOR_SPREAD__3(rgb, b, c, d)
#define COLOR_SPREAD__M(_1, _2, _3, _4, NAME, ...) NAME

#define COLOR_SPREAD(rgb, ...) COLOR_SPREAD__M(__VA_ARGS__, \
	COLOR_SPREAD__4, COLOR_SPREAD__3, COLOR_SPREAD__2, COLOR_SPREAD__1)(rgb, __VA_ARGS__)
#define COLOR_RGB(rgb) COLOR_SPREAD(rgb, 'R', 'G', 'B')
#define COLOR_BGR(rgb) COLOR_SPREAD(rgb, 'B', 'G', 'R')

#define T(c, b) std::get<b>(std::make_tuple(COLOR_SPREAD(c, 'A', 'R', 'G', 'B')))
static_assert(T("#7f74fc6c", 0) == 0x7f, "");
static_assert(T("#7f74fc6c", 1) == 0x74, "");
static_assert(T("#7f74fc6c", 2) == 0xfc, "");
static_assert(T("#7f74fc6c", 3) == 0x6c, "");
static_assert(std::get<0>(std::make_tuple(COLOR_RGB("#74fc6c"))) == 0x74, "");
static_assert(std::get<1>(std::make_tuple(COLOR_RGB("#74fc6c"))) == 0xfc, "");
static_assert(std::get<2>(std::make_tuple(COLOR_RGB("#74fc6c"))) == 0x6c, "");
#undef T

#define APART(c)            (((c)>>24)&0xff)
#define RPART(c)            (((c)>>16)&0xff)
#define GPART(c)            (((c)>>8)&0xff)
#define BPART(c)            ((c)&0xff)
#define MAKERGB__(r,g,b)    uint32_t(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB__(a,r,g,b) uint32_t(((a)<<24)|((r)<<16)|((g)<<8)|(b))
#define MAKERGB(...)        MAKERGB__(__VA_ARGS__)
#define MAKEARGB(...)       MAKEARGB__(__VA_ARGS__)

#ifdef RGB
#undef RGB
#define RGB__(r, g, b) COLORREF(MAKERGB(b, g, r))
#define RGB(...)       RGB__(__VA_ARGS__)
#endif

enum ColorSpace {
	SRGB, OKLAB, OKLCH
};

typedef float ColorP;

typedef struct Color {
	union {
		struct { ColorP r, g, b; } rgb;
		struct { ColorP L, a, b; } lab;
		struct { ColorP L, c, h; } lch;
	};

private:
	ColorSpace type;

	constexpr Color(ColorSpace t, ColorP v1 = 0, ColorP v2 = 0, ColorP v3 = 0) : type(t)
	{
		rgb.r = v1; rgb.g = v2; rgb.b = v3;
	}

	friend constexpr Color rgb(ColorP r, ColorP g, ColorP b);
	friend Color rgb(const Color& c);
	friend void _2rgb(Color& c);
	friend void rgb2oklab(Color& c);
	friend void rgb2oklch(Color& c);

	friend constexpr Color oklch(ColorP L, ColorP c, ColorP h);
	friend Color oklch(const Color& c);
	friend void _2oklch(Color& c);
	friend void oklch2rgb(Color& c);
	friend void oklch2oklab(Color& c);

	friend constexpr Color oklab(ColorP L, ColorP a, ColorP b);
	friend Color oklab(const Color& c);
	friend void _2oklab(Color& c);
	friend void oklab2rgb(Color& c);
	friend void oklab2oklch(Color& c);

	friend Color mix(const Color& a, const Color& b, ColorP mix);
} Color;

constexpr Color rgb(ColorP r, ColorP g, ColorP b)
{
	return { SRGB, r, g, b };
}
Color rgb(const Color& c);
void _2rbg(Color& c);
void rgb2oklab(Color& c);
void rgb2oklch(Color& c);

constexpr Color oklch(ColorP L, ColorP c, ColorP h)
{
	return { OKLCH, L, c, h };
}
Color oklch(const Color& c);
void _2oklch(Color& c);
void oklch2rgb(Color& lch);
void oklch2oklab(Color& lch);

constexpr Color oklab(ColorP L, ColorP a, ColorP b)
{
	return { OKLAB, L, a, b };
}
Color oklab(const Color& c);
void _2oklab(Color& c);
void oklab2rgb(Color& lab);
void oklab2oklch(Color& lab);

Color mix(const Color& a, const Color& b, ColorP mix);

constexpr Color rgb(uint32_t rgb24)
{
	ColorP r = ((rgb24>>16)&0xff)/255.0f;
	ColorP g = ((rgb24>>8 )&0xff)/255.0f;
	ColorP b = ((rgb24    )&0xff)/255.0f;
	return rgb(r, g, b);
}
consteval Color rgb(std::string_view s)
{
	return rgb(str(s));
}
uint32_t rgb24(const Color& c);

}
