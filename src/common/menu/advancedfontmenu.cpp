#include "menu.h"
#include "namedef.h"
#include "v_font.h"
#include <vector>
#include "vm.h"
#include "dobject.h"
#include "c_cvars.h"
#include "gstrings.h"
#include "c_dispatch.h"

FString fontBeingRemapped;

CCMD(editfontmapping)
{
	if (argv.argc() > 0)
	{
		fontBeingRemapped = FString(argv[1]);
		M_StartControlPanel(true);
		M_SetMenu("AdvancedFontOptionsRemapChoiceMenu", -1);
	}
}

DEFINE_ACTION_FUNCTION(DAdvancedFontMenu, FillAdvancedFontMenu)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Clear();
	if (!desc)
	{
		return 0;
	}

	if (desc->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		FFont::MakeFontChoiceCVARs();
		for (int i = 0; i < FFont::GetRemappableFonts().size(); ++i)
		{
			const FFont* const f   = FFont::GetRemappableFonts()[i];
			FString              fontNameString = f->GetName().GetChars();
			FString              cmd            = "editfontmapping " + fontNameString;
			DMenuItemBase *const opt          = CreateOptionMenuItemCommand(f->GetName().GetChars(), cmd);

			desc->mItems.Push(opt);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(DAdvancedFontMenuRemapChoiceMenu, FillAdvancedFontMenuRemapChoices)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(desc, DOptionMenuDescriptor);
	desc->mItems.Clear();
	if (!desc)
	{
		return 0;
	}

	if (desc->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		//FFont::UpdateAdvFontMappingTables();
		FFont::MakeFontChoiceCVARs();
		FString fontNameBeingRemapped = fontBeingRemapped;
		FString langTriplet           = GStrings.GetActiveLangID().name.GetChars();

		std::vector<const FFont*> dynamicFonts;
		const FFont *currentFont = FFont::GetFontListHead();
		if (currentFont && currentFont->IsValidDynamicFont())
		{
			dynamicFonts.push_back(currentFont);
		}
		while (const FFont* nextFont = currentFont->GetNextFont())
		{
			if (nextFont->IsValidDynamicFont())
			{
				dynamicFonts.push_back(nextFont);
			}
			currentFont = nextFont;
		}

		//add a default option
		{
			FString              srcFontName = fontNameBeingRemapped;
			DMenuItemBase *const opt = CreateOptionMenuItemCommand("DEFAULT", FStringf("fontchoice_%s_%s FO-DEFAULT", srcFontName, langTriplet));
			desc->mItems.Push(opt);
		}
		
		for (auto &f : dynamicFonts)
		{
			FString srcFontName = fontNameBeingRemapped;
			FString              tgtFontName = f->GetName().GetChars();
			
			FString              cmd         = FStringf("fontchoice_%s_%s %s", srcFontName, langTriplet, tgtFontName);
			DMenuItemBase *const opt          = CreateOptionMenuItemCommand(f->GetName().GetChars(), cmd);
			desc->mItems.Push(opt);
		}
	}
	return 0;
}
