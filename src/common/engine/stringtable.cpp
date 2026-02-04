/*
** stringtable.cpp
**
** Implements the FStringTable class
**
**---------------------------------------------------------------------------
**
** Copyright 1998-2016 Marisa Heit
** Copyright 2010-2016 Christoph Oelckers
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

#include <cstdint>
#include <iostream>
#include <string.h>

#include "c_cvars.h"
#include "filesystem.h"
#include "i_interface.h"
#include "m_crc32.h"
#include "name.h"
#include "printf.h"
#include "sc_man.h"
#include "stringtable.h"
#include "zstring.h"

EXTERN_CVAR(Int, developer);

//==========================================================================
//
//
//
//==========================================================================

uint32_t FStringTable::GetID(FString lang)
{
	// static TMap<uint32_t, FName> byID;
	static TMap<FName, uint32_t> byStr;

	FName name = lang;

	// ba   749160980 bg  3318017825 by  1070322242 ceb  249967613 chs 3522719042 cht 1335687393 cs  3322219037
	// da  2063461778 de  2106599819 el   492281966 ena 1888929598 enb 3919550084 enc 2660804114 eng 2582995467
	// eni 2118995724 enj 3880001206 enl  237484931 ens 2200942198 ent  491463637 enw 2218947183 enz 4210233042
	// eo  2220814804 es  2422189467 esa 2415569698 esb  385088152 esc 1643432462 esd 4287651757 ese 2291625787
	// esf  295583361 esg 1721306647 esh 4129690502 esi 2166432528 esl 4048279455 esm 2253186825 esn  524662451
	// eso 1749190181 esr  189065980 ess 2084821610 esu 2501934943 esv  204025573 esy 2627089268 esz   94331598
	// fi  1175455522 fr  3430272718 he  3508889223 hr  1391911744 hu  3432150755 id  3208210256 it  2727245620
	// ja  3833511964 jp  2395922670 kab 1687791425 ko   450747482 ms  1485186963 nb   421221538 nl  4272126373
	// no  1739204639 pl   719472250 pt   965664300 ptg  859175243 ro  2178774338 ru  2092928056 sr  4223674586
	// sv  4239256771 tr  3028401693 uk  3388024732 vi   215094643

	uint32_t *idPtr = byStr.CheckKey(name);
	if (idPtr) return *idPtr;

	lang.ToLower();

	uint32_t id = CalcCRC32(lang.GetChars());
	// byID.Insert(id, name);
	byStr.Insert(name, id);

	return id;
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadStrings (FileSys::FileSystem& fileSystem, const char *language)
{
	int lastlump, lump;

	allStrings.Clear();
	lastlump = 0;
	while ((lump = fileSystem.FindLump("LMACROS", &lastlump)) != -1)
	{
		auto lumpdata = fileSystem.ReadFile(lump);
		readMacros(lumpdata.string(), lumpdata.size());
	}

	lastlump = 0;
	while ((lump = fileSystem.FindLump ("LANGUAGE", &lastlump)) != -1)
	{
		auto lumpdata = fileSystem.ReadFile(lump);
		auto filenum = fileSystem.GetFileContainer(lump);

		if (!ParseLanguageCSV(filenum, lumpdata.string(), lumpdata.size()))
 			LoadLanguage (filenum, lumpdata.string(), lumpdata.size());
	}
	UpdateLanguage(language);
	allMacros.Clear();
}


//==========================================================================
//
// This was tailored to parse CSV as exported by Google Docs.
//
//==========================================================================


TArray<TArray<FString>> FStringTable::parseCSV(const char* buffer, size_t size)
{
	const size_t bufLength = size;
	TArray<TArray<FString>> data;
	TArray<FString> row;
	TArray<char> cell;
	bool quoted = false;

	/*
			auto myisspace = [](int ch) { return ch == '\t' || ch == '\r' || ch == '\n' || ch == ' '; };
			while (*vcopy && myisspace((unsigned char)*vcopy)) vcopy++;	// skip over leaading whitespace;
			auto vend = vcopy + strlen(vcopy);
			while (vend > vcopy && myisspace((unsigned char)vend[-1])) *--vend = 0;	// skip over trailing whitespace
	*/

	for (size_t i = 0; i < bufLength; ++i)
	{
		if (buffer[i] == '"')
		{
			// Double quotes inside a quoted string count as an escaped quotation mark.
			if (quoted && i < bufLength - 1 && buffer[i + 1] == '"')
			{
				cell.Push('"');
				i++;
			}
			else if (cell.Size() == 0 || quoted)
			{
				quoted = !quoted;
			}
		}
		else if (buffer[i] == ',')
		{
			if (!quoted)
			{
				cell.Push(0);
				ProcessEscapes(cell.Data());
				row.Push(cell.Data());
				cell.Clear();
			}
			else
			{
				cell.Push(buffer[i]);
			}
		}
		else if (buffer[i] == '\r')
		{
			// Ignore all CR's.
		}
		else if (buffer[i] == '\n' && !quoted)
		{
			cell.Push(0);
			ProcessEscapes(cell.Data());
			row.Push(cell.Data());
			data.Push(std::move(row));
			cell.Clear();
		}
		else
		{
			cell.Push(buffer[i]);
		}
	}

	// Handle last line without linebreak
	if (cell.Size() > 0 || row.Size() > 0)
	{
		cell.Push(0);
		ProcessEscapes(cell.Data());
		row.Push(cell.Data());
		data.Push(std::move(row));
	}
	return data;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::readMacros(const char* buffer, size_t size)
{
	auto data = parseCSV(buffer, size);

	allMacros.Clear();
	for (unsigned i = 1; i < data.Size(); i++)
	{
		auto macroname = data[i][0];
		FName name = macroname.GetChars();

		StringMacro macro;

		for (int k = 0; k < 4; k++)
		{
			macro.Replacements[k] = data[i][k+2];
		}
		allMacros.Insert(name, macro);
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool FStringTable::ParseLanguageCSV(int filenum, const char* buffer, size_t size)
{
	if (size < 11) return false;
	if (strnicmp(buffer, "default,", 8) && strnicmp(buffer, "identifier,", 11 )) return false;
	auto data = parseCSV(buffer, size);

	int labelcol = -1;
	int filtercol = -1;
	TArray<std::pair<int, uint32_t>> langrows;
	bool hasDefaultEntry = false;

	if (data.Size() > 0)
	{
		for (unsigned column = 0; column < data[0].Size(); column++)
		{
			auto &entry = data[0][column];
			if (entry.CompareNoCase("filter") == 0)
			{
				filtercol = column;
			}
			else if (entry.CompareNoCase("identifier") == 0)
			{
				labelcol = column;
			}
			else
			{
				auto languages = entry.Split(" ", FString::TOK_SKIPEMPTY);
				for (auto &lang : languages)
				{
					if (lang.CompareNoCase("default") == 0)
					{
						langrows.Push(std::make_pair(column, default_table));
						hasDefaultEntry = true;
					}
					else if (lang.Len() < 4)
					{
						langrows.Push(std::make_pair(column, GetID(lang)));
					}
				}
			}
		}

		for (unsigned i = 1; i < data.Size(); i++)
		{
			auto &row = data[i];
			if (filtercol > -1)
			{
				auto filterstr = row[filtercol];
				if (filterstr.IsNotEmpty())
				{
					bool ok = false;
					if (sysCallbacks.CheckGame)
					{
						auto filter = filterstr.Split(" ", FString::TOK_SKIPEMPTY);
						for (auto& entry : filter)
						{
							if (sysCallbacks.CheckGame(entry.GetChars()))
							{
								ok = true;
								break;
							}
						}
					}
					if (!ok) continue;
				}
			}

			row[labelcol].StripLeftRight();
			FName strName = row[labelcol].GetChars();
			if (hasDefaultEntry)
			{
				DeleteForLabel(filenum, strName);
			}
			for (auto &langentry : langrows)
			{
				auto str = row[langentry.first];
				if (str.Len() > 0)
				{
					InsertString(filenum, langentry.second, strName, str);
				}
				else
				{
					DeleteString(langentry.second, strName);
				}
			}
		}
	}
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::LoadLanguage (int lumpnum, const char* buffer, size_t size)
{
	bool errordone = false;
	TArray<uint32_t> activeMaps;
	FScanner sc;
	bool hasDefaultEntry = false;

	sc.OpenMem(fileSystem.GetFileFullPath(lumpnum).c_str(), buffer, (int)size);
	sc.SetCMode (true);
	while (sc.GetString ())
	{
		if (sc.Compare ("["))
		{ // Process language identifiers
			activeMaps.Clear();
			hasDefaultEntry = false;
			sc.MustGetString ();
			do
			{
				size_t len = sc.StringLen;

				if (len < 1)
				{
					sc.ScriptError ("The language code may not be empty.");
				}
				if (len == 1 && sc.String[0] == '~')
				{
					// deprecated and ignored
					sc.ScriptMessage("Deprecated option '~' found in language list");
					sc.MustGetString ();
					continue;
				}
				if (len == 1 && sc.String[0] == '*')
				{
					activeMaps.Clear();
					activeMaps.Push(global_table);
				}
				else if (len == 7 && stricmp (sc.String, "default") == 0)
				{
					activeMaps.Clear();
					activeMaps.Push(default_table);
					hasDefaultEntry = true;
				}
				else if (activeMaps.Size() != 1 || (activeMaps[0] != default_table && activeMaps[0] != global_table))
				{
					activeMaps.Push(GetID(sc.String));
				}

				sc.MustGetString ();
			} while (!sc.Compare ("]"));
		}
		else
		{ // Process string definitions.
			if (activeMaps.Size() == 0)
			{
				// LANGUAGE lump is bad. We need to check if this is an old binary
				// lump and if so just skip it to allow old WADs to run which contain
				// such a lump.
				if (!sc.isText())
				{
					if (!errordone) Printf("Skipping binary 'LANGUAGE' lump.\n"); 
					errordone = true;
					return;
				}
				sc.ScriptError ("Found a string without a language specified.");
			}

			bool skip = false;
			if (sc.Compare("$"))
			{
				sc.MustGetStringName("ifgame");
				sc.MustGetStringName("(");
				sc.MustGetString();
				skip |= (!sysCallbacks.CheckGame || !sysCallbacks.CheckGame(sc.String));
				sc.MustGetStringName(")");
				sc.MustGetString();

			}

			FName strName (sc.String);
			sc.MustGetStringName ("=");
			sc.MustGetString ();
			FString strText (sc.String, ProcessEscapes (sc.String));
			sc.MustGetString ();
			while (!sc.Compare (";"))
			{
				ProcessEscapes (sc.String);
				strText += sc.String;
				sc.MustGetString ();
			}
			if (!skip)
			{
				if (hasDefaultEntry)
				{
					DeleteForLabel(lumpnum, strName);
				}
				// Insert the string into all relevant tables.
				for (auto map : activeMaps)
				{
					InsertString(lumpnum, map, strName, strText);
				}
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::DeleteString(uint32_t langid, FName label)
{
	allStrings[langid].Remove(label);
}

//==========================================================================
//
// This deletes all older entries for a given label. This gets called
// when a string in the default table gets updated. 
//
//==========================================================================

void FStringTable::DeleteForLabel(int filenum, FName label)
{
	decltype(allStrings)::Iterator it(allStrings);
	decltype(allStrings)::Pair *pair;

	while (it.NextPair(pair))
	{
		auto entry = pair->Value.CheckKey(label);
		if (entry && entry->filenum < filenum)
		{
			pair->Value.Remove(label);
		}
	}

}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::InsertString(int filenum, uint32_t langid, FName label, const FString &string)
{
	const char *strlangid = (const char *)&langid;
	TableElement te = { filenum, { string, string, string, string } };
	ptrdiff_t index;
	while ((index = te.strings[0].IndexOf("@[")) >= 0)
	{
		auto endindex = te.strings[0].IndexOf(']', index);
		if (endindex == -1)
		{
			Printf("Bad macro in %s : %s\n", strlangid, label.GetChars());
			break;
		}
		FString macroname(te.strings[0].GetChars() + index + 2, endindex - index - 2);
		FStringf replacee("@[%s]", macroname.GetChars());
		FName lookupname(macroname.GetChars(), true);
		auto replace = allMacros.CheckKey(lookupname);
		for (int i = 0; i < 4; i++)
		{
			const char *replacement = replace? replace->Replacements[i].GetChars() : "";
			te.strings[i].Substitute(replacee, replacement);
		}
	}
	allStrings[langid].Insert(label, te);
}

//==========================================================================
//
//
//
//==========================================================================

void FStringTable::UpdateLanguage(const char *language)
{
	if (language) activeLanguage = language;
	else language = activeLanguage.GetChars();
	size_t langlen = strlen(language);

	uint32_t LanguageID = (langlen < 2) ? GetID("en-us"): GetID(language);

	currentLanguageSet.Clear();

	auto checkone = [&](uint32_t lang_id)
	{
		auto list = allStrings.CheckKey(lang_id);
		if (list && currentLanguageSet.FindEx([&](const auto &element) { return element.first == lang_id; }) == currentLanguageSet.Size())
			currentLanguageSet.Push(std::make_pair(lang_id, list));
	};

	checkone(override_table);
	checkone(global_table);
	checkone(LanguageID);
	checkone(default_table);
}

//==========================================================================
//
// Replace \ escape sequences in a string with the escaped characters.
//
//==========================================================================

size_t FStringTable::ProcessEscapes (char *iptr)
{
	char *sptr = iptr, *optr = iptr, c;

	while ((c = *iptr++) != '\0')
	{
		if (c == '\\')
		{
			c = *iptr++;
			if (c == 'n')
				c = '\n';
			else if (c == 'c')
				c = TEXTCOLOR_ESCAPE;
			else if (c == 'r')
				c = '\r';
			else if (c == 't')
				c = '\t';
			else if (c == 'x')
			{
				c = 0;
				for (int i = 0; i < 2; i++)
				{
					char cc = *iptr++;
					if (cc >= '0' && cc <= '9')
						c = (c << 4) + cc - '0';
					else if (cc >= 'a' && cc <= 'f')
						c = (c << 4) + 10 + cc - 'a';
					else if (cc >= 'A' && cc <= 'F')
						c = (c << 4) + 10 + cc - 'A';
					else
					{
						iptr--;
						break;
					}
				}
				if (c == 0) continue;
			}
			else if (c == '\n')
				continue;
		}
		*optr++ = c;
	}
	*optr = '\0';
	return optr - sptr;
}

//==========================================================================
//
// Checks if the given key exists in any one of the default string tables that are valid for all languages.
// To replace IWAD content this condition must be true.
//
//==========================================================================

bool FStringTable::exists(const char *name)
{
	if (name == nullptr || *name == 0)
	{
		return false;
	}
	FName nm(name, true);
	if (nm != NAME_None)
	{
		uint32_t defaultStrings[] = { default_table, global_table, override_table };

		for (auto mapid : defaultStrings)
		{
			auto map = allStrings.CheckKey(mapid);
			if (map)
			{
				auto item = map->CheckKey(nm);
				if (item) return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// Finds a string by name and returns its value
//
//==========================================================================

const char *FStringTable::CheckString(const char *name, uint32_t *langtable, int gender) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	if (gender == -1) gender = defaultgender;
	if (gender < 0 || gender > 3) gender = 0;
	FName nm(name, true);
	if (nm != NAME_None)
	{
		TableElement* bestItem = nullptr;
		for (auto map : currentLanguageSet)
		{
			auto item = map.second->CheckKey(nm);
			if (item)
			{
				if (bestItem && bestItem->filenum > item->filenum)
				{
					// prioritize content from later files, even if the language doesn't fully match.
					// This is mainly for Dehacked content.
					continue;
				}
				if (langtable) *langtable = map.first;
				auto c = item->strings[gender].GetChars();
				if (c && *c == '$' && c[1] == '$')
					c = CheckString(c + 2, langtable, gender);
				return c;
			}
		}
	}
	return nullptr;
}

//==========================================================================
//
// Finds a string by name in a given language without attempting any substitution
//
//==========================================================================

const char *FStringTable::GetLanguageString(const char *name, uint32_t langtable, int gender) const
{
	if (name == nullptr || *name == 0)
	{
		return nullptr;
	}
	if (gender == -1) gender = defaultgender;
	if (gender < 0 || gender > 3) gender = 0;
	FName nm(name, true);
	if (nm != NAME_None)
	{
		auto map = allStrings.CheckKey(langtable);
		if (map == nullptr) return nullptr;
		auto item = map->CheckKey(nm);
		if (item)
		{
			return item->strings[gender].GetChars();
		}
	}
	return nullptr;
}

bool FStringTable::MatchDefaultString(const char *name, const char *content) const
{
	// This only compares the first line to avoid problems with bad linefeeds. For the few cases where this feature is needed it is sufficient.
	auto c = GetLanguageString(name, FStringTable::default_table);
	if (!c) return false;

	// Check a secondary key, in case the text comparison cannot be done due to needed orthographic fixes (see Harmony's exit text)
	FStringf checkkey("%s_CHECK", name);
	auto cc = GetLanguageString(checkkey.GetChars(), FStringTable::default_table);
	if (cc) c = cc;

	return (c && !strnicmp(c, content, strcspn(content, "\n\r\t")));
}

//==========================================================================
//
// Finds a string by name and returns its value. If the string does
// not exist, returns the passed name instead.
//
//==========================================================================
const char *FStringTable::GetString(const char *name) const
{
	const char *str = CheckString(name);

	if (developer != 0 && !str)
	{
		static TMap<FName, bool> missed;

		FName fname = name;
		if (!missed.CheckKey(fname))
		{
			missed.Insert(fname, true);
			DPrintf(DMSG_WARNING, "Translation not found '%s'\n", name);
		}
	}

	return str ? str : name;
}


//==========================================================================
//
// Find a string with the same exact text. Returns its name.
// This does not need to check genders, it is only used by
// Dehacked on the English table for finding stock strings.
//
//==========================================================================

const char *StringMap::MatchString (const char *string) const
{
	StringMap::ConstIterator it(*this);
	StringMap::ConstPair *pair;

	while (it.NextPair(pair))
	{
		if (pair->Value.strings[0].CompareNoCase(string) == 0)
		{
			return pair->Key.GetChars();
		}
	}
	return nullptr;
}

FStringTable GStrings;


