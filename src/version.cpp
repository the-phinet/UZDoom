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

#include <charconv>
#include <climits>
#include <cstring>
#include <string_view>

#include "basics.h"
#include "gitinfo.h"
#include "version.h"
#include "versioninfo.h"
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
		return GetCurrentVersion();
	}
}

VersionInfo GetCurrentEngineVersion()
{
	return MakeVersion(ENG_MAJOR, ENG_MINOR, ENG_REVISION);
}

bool IsValidExtension(const char *data, size_t count)
{
	if (!data || !count) return false;
	std::string_view s(data, count);
	auto p = s.find('\0');
	if (p == std::string_view::npos) return false;
	if (p >= count) return false;
	return std::all_of(s.begin(), s.begin()+p, [](char c) {
		return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == '.';
	});
}

VersionInfo::VersionInfo(const char *string)
{
	major = minor = revision = distance = 0;
	std::memset(extension, 0, sizeof(extension));
	std::memset(commit, 0, sizeof(commit));

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
	{ // this parses specifically only what `VersionInfo::operator FString()` can emit
		if (!IsValidExtension(endp+1, strlen(endp+1)+1)) return;
		std::string ext = endp+1;
		size_t size = ext.size();
		if (size <= sizeof(commit)) return; // at minimum 0-0000000
		bool mod = ext[size-1] == 'm';
		if (mod)
		{
			if (ext[size-2] != '-' && size <= sizeof(commit)+2) return; // at minimum 0-0000000-m
			size -= 2;
		}
		size_t mid = size - sizeof(commit);
		if (ext[mid] != '-') return;

		auto dst = ext.substr(0, mid);
		if (!std::all_of(dst.begin(), dst.end(), [](char c) { return ('0' <= c && c <= '9'); })) return;

		auto cmt = ext.substr(mid+1, size-mid-1);
		if (!std::all_of(cmt.begin(), cmt.end(), [](char c) { return ('0' <= c && c <= '9') || ('a' <= c && c <= 'f'); })) return;

		distance = (int16_t)clamp<unsigned long long>(strtoull(dst.c_str(), nullptr, 10), 0, USHRT_MAX);
		cmt.copy(commit, sizeof(commit));
		if (mod) distance = -(distance+1);
	}
}

std::strong_ordering VersionInfo::operator <=> (const VersionInfo& o) const
{
	assert(IsValidExtension(extension, sizeof(extension)));
	assert(IsValidExtension(o.extension, sizeof(o.extension)));

	if (auto cmp = major <=> o.major; cmp != 0) return cmp;
	if (auto cmp = minor <=> o.minor; cmp != 0) return cmp;
	if (auto cmp = revision <=> o.revision; cmp != 0) return cmp;

	std::string_view ext_a(extension);
	std::string_view ext_b(o.extension);

	// version with extension (1.0.0-rc1) comes before one without (1.0.0)
	if (auto cmp = ext_a.empty() <=> ext_b.empty(); cmp != 0) return cmp;

	// converts string to natural number, returns -1 if not possible
	auto Number = [](std::string_view s)->int {
		if (s.empty()) return -1;
		int v = 0;
		auto [p, e] = std::from_chars(s.data(), s.data() + s.size(), v);
		if (e == std::errc{} && p == s.data() + s.size() && v >= 0) return v;
		return -1;
	};

	// https://semver.org/#spec-item-11 (point 4)
	size_t start_a = 0, start_b = 0, end_a = 0, end_b = 0, len_a = ext_a.size(), len_b = ext_b.size();
	std::string_view sub_a, sub_b;
	int num_a, num_b;
	while (start_a <= len_a && start_b <= len_b)
	{
		while (end_a < len_a && ext_a[end_a] != '.') end_a++;
		while (end_b < len_b && ext_b[end_b] != '.') end_b++;
		sub_a = ext_a.substr(start_a, end_a-start_a);
		sub_b = ext_b.substr(start_b, end_b-start_b);
		num_a = Number(sub_a);
		num_b = Number(sub_b);
		if (num_a < 0 || num_b < 0)
		{
			if (auto cmp = (num_a < 0) <=> (num_b < 0); cmp != 0) return cmp;
			if (auto cmp = sub_a <=> sub_b; cmp != 0) return cmp;
		}
		else
		{
			if (auto cmp = num_a <=> num_b; cmp != 0) return cmp;
		}
		end_a++;
		end_b++;
		start_a = end_a;
		start_b = end_b;
	}
	if (auto cmp = (start_a <= len_a) <=> (start_b <= len_b); cmp != 0) return cmp;

	// not actually part of semvers
	return (distance >= 0 && o.distance >= 0)
		? distance <=> o.distance
		: o.distance <=> distance; // if one is negative, it has been edited. The edited one is "newer"
}

void VersionInfo::operator=(const char *string)
{
	(*this) = VersionInfo(string);
}

VersionInfo::operator FString() const
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
