/*
** version.cpp
**
** Functions to get build info
**
**---------------------------------------------------------------------------
**
** Copyright 1999-2016 Marisa Heit
** Copyright 2006-2016 Christoph Oelckers
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

#include <climits>
#include <cstring>

#include "gitinfo.h"
#include "version.h"
#include "versioninfo.h"
#include "basics.h"
#include "zstring.h"

//==========================================================================
//
// <Tag>-<Distance>-g<commit>
//
//==========================================================================

const char *GetVersionString()
{
	static FString version = FString{GetCurrentVersion()};

	return version.GetChars();
}

//==========================================================================
//
// <commit>
//
//==========================================================================

const char *GetGitHash()
{
	return GIT_HASH;
}

//==========================================================================
//
// ISO 8601
//
//==========================================================================

const char *GetGitTime()
{
	return GIT_TIME;
}

//==========================================================================
//
// Closest git tag
//
//==========================================================================

const char *GetGitTag()
{
	return GIT_TAG;
}

//==========================================================================
//
// Distance to closest git tag
//
//==========================================================================

int GetGitDistance()
{
	return GIT_DISTANCE;
}

VersionInfo GetCurrentVersion()
{
	static VersionInfo version = ([]() {
		VersionInfo v = { VER_MAJOR, VER_MINOR, VER_REVISION };
		std::string_view hash = GIT_HASH;
		if (hash != "0000000")
		{
			v = VersionInfo{GIT_TAG};
			v.distance = GIT_DISTANCE;
			hash.substr(0,sizeof(v.commit)-1).copy(v.commit, sizeof(v.commit)-1);
			v.commit[sizeof(v.commit)-1] = '\0';
		}
		assert(v.major == VER_MAJOR);
		assert(v.minor == VER_MINOR);
		assert(v.revision == VER_REVISION);
		return v;
	})();

	return version;
}

VersionInfo GetCurrentVersionForUpdate(UpdateChannel channel)
{
#ifdef DEBUG_FORCE_UPDATE
	return VersionInfo(1,0,0,0);
#endif

	switch(channel)
	{
	case UpdateChannel::STABLE:
	case UpdateChannel::RELEASE_CANDIDATE:
		// no releases can be made when git distance is not 0
	case UpdateChannel::PREVIEW:
	case UpdateChannel::TESTING:
		return MakeVersion2(GIT_DESCRIPTION, GIT_TAG);
	}
}

VersionInfo GetCurrentEngineVersion()
{
	return MakeVersion(ENG_MAJOR, ENG_MINOR, ENG_REVISION);
}

VersionInfo::VersionInfo(const char *string)
{
	major = minor = revision = distance = 0;
	std::memset(extension, 0, sizeof(extension));

	if (!string || *string == '\0') return;

	char *endp;

	major = (int16_t)clamp<unsigned long long>(strtoull(string, &endp, 10), 0, USHRT_MAX);
	if (endp && *endp == '.')
	{
		minor = (int16_t)clamp<unsigned long long>(strtoull(endp + 1, &endp, 10), 0, USHRT_MAX);
	}

	if (endp && *endp == '.')
	{
		revision = (int16_t)clamp<unsigned long long>(strtoull(endp + 1, &endp, 10), 0, USHRT_MAX);
	}

	if (endp && *endp == '-')
	{
		endp++;

		size_t i, max_chars = (sizeof(extension) / sizeof(extension[0])) - 1;

		for (i = 0; i < max_chars && endp[i] && endp[i] != '+'; i++)
		{
			extension[i] = endp[i];
		}
		extension[i] = '\0';
		endp+=i;
	}

	if (endp && *endp == '+')
	{
		distance = (int16_t)clamp<unsigned long long>(strtoull(endp + 1, &endp, 10), 0, USHRT_MAX);
	}
}

VersionInfo MakeVersion2(const char *version, const char *base)
{
	auto v = VersionInfo(base);

	if (!version) return v;

	auto verlen = strlen(version);
	auto baselen = strlen(base);

	if (verlen <= baselen+1) return v; // cannot have extra data
	if (strncmp(base, version, baselen) != 0) return v; // not from this tag // FIXME: this should throw

	version += baselen + 1;
	if ('0' <= *version && *version <= '9')
	{
		v.distance = (uint32_t)clamp<unsigned long long>(strtoull(version, nullptr, 10), 0, UINT32_MAX);
	}

	return v;
}

void VersionInfo::operator=(const char *string)
{
	(*this) = VersionInfo(string);
}

VersionInfo::operator FString()
{
	FString tmp = FStringf("%u.%u.%u", major, minor, revision);
	if (*extension) tmp.AppendFormat("-%s", extension);
	auto dist = distance;
	if (dist != 0)
	{
		bool modified = dist < 0;
		if (modified) dist = -(dist+1);
		tmp.AppendFormat("+%d-%s%s", dist, commit, modified? "-m": "");
	}
	return tmp;
}
