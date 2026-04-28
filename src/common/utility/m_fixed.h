/*
** m_fixed.h
**
**
**
**---------------------------------------------------------------------------
**
** Copyright 2009-2016 Marisa Heit
** Copyright 2017-2025 GZDoom Maintainers and Contributors
** Copyright 2025-2026 UZDoom Maintainers and Contributors
**
** SPDX-License-Identifier: GPL-3.0-or-later
**
**---------------------------------------------------------------------------
**
** Code written prior to 2026 is also licensed under:
**
** SPDX-License-Identifier: BSD-3-Clause
**
**---------------------------------------------------------------------------
**
*/

#pragma once

#include "basics.h"

#include "printf.h"
#include "xs_Float.h"

#define TEST(A, B, V, b) \
	if (A != B) Printf("%s:%d, %d %f %d %d\n", __FILE__, __LINE__, b, V, A, B);

inline constexpr int32_t MulScale(int32_t a, int32_t b, int32_t shift) { return (int32_t)(((int64_t)a * b) >> shift); }
inline constexpr int32_t DMulScale(int32_t a, int32_t b, int32_t c, int32_t d, int32_t shift) { return (int32_t)(((int64_t)a * b + (int64_t)c * d) >> shift); }
inline constexpr int32_t DivScale(int32_t a, int32_t b, int shift) { return (int32_t)(((int64_t)a << shift) / b); }
inline constexpr int64_t DivScaleL(int64_t a, int64_t b, int shift) { return ((a << shift) / b); }

template<int b = 16>
static fixed_t FloatToFixed(double f)
{
	auto A = xs_Fix<b>::ToFix(f);
	auto B = RoundHalfEven(f * (1 << b));
	TEST(A, B, f, b);
	return RoundHalfEven(f * (1 << b));
}

static fixed_t FloatToFixed(double f, int b)
{
	auto A = xs_ToFixed(b, f);
	auto B = RoundHalfEven((long)(f * (1 << b)));
	TEST(A, B, f, b);
	return RoundHalfEven(f * (1 << b));
}

template<int b = 16>
inline constexpr fixed_t IntToFixed(int32_t f)
{
	return f << b;
}

template<int b = 16>
inline constexpr double FixedToFloat(fixed_t f)
{
	return f * (1. / (1 << b));
}

inline constexpr double FixedToFloat(fixed_t f, int b)
{
	return f * (1. / (1 << b));
}

template<int b = 16>
inline constexpr int32_t FixedToInt(fixed_t f)
{
	return (f + (1 << (b-1))) >> b;
}

static inline unsigned FloatToAngle(double f)
{
	return RoundHalfEven((f)* (0x40000000 / 90.));
}

#define FLOAT2FIXED(f) FloatToFixed(f)
#define FIXED2FLOAT(f) float(FixedToFloat(f))
#define FIXED2DBL(f)   FixedToFloat(f)

#undef TEST
