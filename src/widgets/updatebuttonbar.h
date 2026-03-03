/*
** updatebuttonbar.h
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

#include <zwidget/core/widget.h>

class LauncherWindow;
class PushButton;

class UpdateButtonBar : public Widget
{
  public:
	UpdateButtonBar(LauncherWindow *parent);
	void UpdateLanguage();

	double GetPreferredHeight() const;

	void CheckForUpdate(bool autoupdate);

  private:
	void OnGeometryChanged() override;
	void OnUpdateButtonClicked();

	LauncherWindow *GetLauncher() const;

	PushButton *UpdateButton = nullptr;
};
