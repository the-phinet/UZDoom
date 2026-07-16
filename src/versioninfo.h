/*
** versioninfo.h
**
**
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

#include <cstring>
#include <string_view>

#include "basics.h"

class FString;

struct VersionInfo
{
	uint16_t major;
	uint16_t minor;
	uint32_t revision;
	int32_t distance;
	char extension[16];
	char commit[8];

	constexpr VersionInfo() = default;
	constexpr VersionInfo(uint16_t _major, uint16_t _minor, uint32_t _revision = 0, int32_t _distance = 0)
		: major(_major), minor(_minor), revision(_revision), distance(_distance), extension{}, commit{}
	{}
	explicit VersionInfo(const char *);

	constexpr bool operator <=(const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::greater;
	}
	constexpr bool operator >=(const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::less;
	}
	constexpr bool operator > (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::greater;
	}
	constexpr bool operator < (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::less;
	}
	constexpr bool operator == (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::equal;
	}
	constexpr bool operator != (const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::equal;
	}

	constexpr std::strong_ordering operator <=> (const VersionInfo& o) const
	{
		auto validate = [](const VersionInfo& v)
		{
			std::string_view s(v.extension, sizeof(v.extension));
			auto p = s.find('\0');
			if (p == std::string_view::npos) return false;
			if (p >= sizeof(extension)) return false;
			return std::all_of(s.begin(), s.begin()+p, [](char c) {
				return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == '.';
			});
		};
		assert(validate(*this));
		assert(validate(o));

		if (auto cmp = major <=> o.major; cmp != 0) return cmp;
		if (auto cmp = minor <=> o.minor; cmp != 0) return cmp;
		if (auto cmp = revision <=> o.revision; cmp != 0) return cmp;

		std::string_view ext_a(extension);
		std::string_view ext_b(o.extension);

		// version with extension (1.0.0-rc1) comes before one without (1.0.0)
		if (auto cmp = ext_a.empty() <=> ext_b.empty(); cmp != 0) return cmp;
		// FIXME: update me to handle semver sorting https://semver.org/#spec-item-11 (bullet 4)
		// we need to do this if we ever have more than 10 numbered pre-releases lol
		if (auto cmp = ext_a <=> ext_b; cmp != 0) return cmp;

		if (distance >= 0 && o.distance >= 0)
		{
			return distance <=> o.distance;
		}
		else
		{ // if one is negative, it has been edited. The edited one is "newer"
			return o.distance <=> distance;
		}
	}

	void operator=(const char* string);
	explicit operator FString();
};

// Cannot be a constructor because Lemon would puke on it.
constexpr VersionInfo MakeVersion(unsigned int ma, unsigned int mi, unsigned int re = 0)
{
	return{ (uint16_t)ma, (uint16_t)mi, (uint32_t)re };
}

VersionInfo MakeVersion2(const char *, const char *);

VersionInfo GetCurrentVersion();

VersionInfo GetCurrentVersionForUpdate(UpdateChannel channel);

VersionInfo GetCurrentEngineVersion();
