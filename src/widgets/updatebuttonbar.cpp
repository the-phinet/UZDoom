/*
** updatebuttonbar.cpp
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

#include "updatebuttonbar.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include <zwidget/widgets/pushbutton/pushbutton.h>

UpdateButtonBar::UpdateButtonBar(LauncherWindow *parent) : Widget(parent)
{
	this->SetStyleColor("background-color", Colorf(0, 0.3, 0.5, 1.0));
	UpdateButton = new PushButton(this);

	UpdateButton->OnClick = [=]() { OnUpdateButtonClicked(); };
}

void UpdateButtonBar::UpdateLanguage()
{
	UpdateButton->SetText(GStrings.GetString("PICKER_BUTTON_UPDATE"));
}

double UpdateButtonBar::GetPreferredHeight() const
{
	return 20.0 + UpdateButton->GetPreferredHeight();
}

void UpdateButtonBar::OnGeometryChanged()
{
	UpdateButton->SetFrameGeometry(20.0, 0.0, 120.0, UpdateButton->GetPreferredHeight());
}

void UpdateButtonBar::OnUpdateButtonClicked()
{
	//DoUpdate();
	GetLauncher()->Close();

	return;
}

LauncherWindow* UpdateButtonBar::GetLauncher() const
{
	return static_cast<LauncherWindow*>(Parent());
}

void UpdateButtonBar::CheckForUpdate(bool autoupdate)
{
	/*
	if (GetReleaseData())
	{
		if (UpdateAvailable())
		{
			Show();
		}
		else
		{
			Hide();
		}
	}
	*/
}
