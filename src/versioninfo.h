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
	constexpr VersionInfo(
		uint16_t _major,
		uint16_t _minor,
		uint32_t _revision = 0,
		int32_t _distance = 0,
		const char *_ext = "",
		const char *_cmt = ""
	): major(_major), minor(_minor), revision(_revision), distance(_distance), extension{}, commit{}
	{
		for (size_t i = 0; i < sizeof(extension)-1 && _ext && _ext[i]; i++) extension[i] = _ext[i];
		for (size_t i = 0; i < sizeof(commit)-1 && _cmt && _cmt[i]; i++) commit[i] = _cmt[i];
	}
	explicit VersionInfo(const char *);

	bool operator <=(const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::greater;
	}
	bool operator >=(const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::less;
	}
	bool operator > (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::greater;
	}
	bool operator < (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::less;
	}
	bool operator == (const VersionInfo& o) const
	{
		return operator<=>(o) == std::strong_ordering::equal;
	}
	bool operator != (const VersionInfo& o) const
	{
		return operator<=>(o) != std::strong_ordering::equal;
	}

	std::strong_ordering operator <=> (const VersionInfo& o) const;

	void operator=(const char* string);
	explicit operator FString() const;
};

// Cannot be a constructor because Lemon would puke on it.
constexpr VersionInfo MakeVersion(unsigned int ma, unsigned int mi, unsigned int re = 0)
{
	return{ (uint16_t)ma, (uint16_t)mi, (uint32_t)re };
}

VersionInfo GetCurrentVersion();

VersionInfo GetCurrentVersionForUpdate(UpdateChannel channel);

VersionInfo GetCurrentEngineVersion();
