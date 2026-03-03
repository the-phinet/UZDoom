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

		int GetMargin() { return 4; }

	protected:
		void OnPaint(Canvas* canvas) override;
		void OnMouseMove(const Point& pos) override;
		void OnMouseLeave() override;
		bool OnMouseDown(const Point& pos, InputKey key) override;
		bool OnMouseUp(const Point& pos, InputKey key) override;
		void OnUpdateButtonClicked();

		LauncherWindow *GetLauncher() const;

		const char * text = "";

		bool close_highlighted = false;
		bool pressed = false;
		bool pressed_hover = false;
		bool close_pressed = false;
		bool close_pressed_hover = false;

		std::shared_ptr<Image> arrow;
		std::shared_ptr<Image> close;
};
