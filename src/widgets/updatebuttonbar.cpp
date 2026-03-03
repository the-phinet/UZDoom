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
constexpr int bar_height = 30;
constexpr int close_margin = 6;
constexpr int arrow_margin = 0;

UpdateButtonBar::UpdateButtonBar(LauncherWindow *parent) : Widget(parent)
{
	SetStyleColor("bg-default-color", Colorf(0.0f, 0.3f, 0.5f, 1.0f));
	SetStyleColor("bg-highlight-color", Colorf(0.2f, 0.5f, 0.75f, 1.0f));
	SetStyleColor("bg-press-color", Colorf(0.0f, 0.2f, 0.333f, 1.0f));
	SetStyleColor("close-highlight-color", Colorf(0.9f, 0.3f, 0.2f, 1.0f));
	SetStyleColor("close-press-color", Colorf(0.6f, 0.15f, 0.1f, 1.0f));

	SetStyleColor("background-color", GetStyleColor("bg-default-color"));
	SetStyleColor("color", Colorf(1.0f, 1.0f, 1.0f, 1.0f));
	arrow = Image::LoadResource("ui/arrow.png");
	close = Image::LoadResource("ui/close.png");
}

void UpdateButtonBar::UpdateLanguage()
{
	UpdateButton->SetText(GStrings.GetString("PICKER_BUTTON_UPDATE"));
	text = GStrings.GetString("PICKER_BUTTON_UPDATE");
}

void UpdateButtonBar::OnPaint(Canvas* canvas)
{
	canvas->fillRect(Rect(0, 0, bar_height, bar_height), close_pressed ? GetStyleColor("close-press-color") : close_highlighted ? GetStyleColor("close-highlight-color") : GetStyleColor("bg-default-color"));

	Rect box = canvas->measureText(text);
	canvas->drawText(Point((GetWidth() - bar_height - box.width) * 0.5, (bar_height * 0.5) + (box.height * 0.35)), GetStyleColor("color"), text);

	canvas->drawImage(close, Rect(close_margin, close_margin, bar_height - (close_margin * 2), bar_height - (close_margin * 2)));
	canvas->drawImage(arrow, Rect(GetWidth() - (bar_height - arrow_margin), arrow_margin, bar_height - (arrow_margin * 2), bar_height - (arrow_margin * 2)));
}

void UpdateButtonBar::OnMouseMove(const Point& pos)
{
	if(pressed || close_pressed) return;

	if(pos.x > bar_height)
	{
		SetStyleColor("background-color", GetStyleColor("bg-highlight-color"));
		close_highlighted = false;
	}
	else
	{
		SetStyleColor("background-color", GetStyleColor("bg-default-color"));
		close_highlighted = true;
	}

	Update();
}

void UpdateButtonBar::OnMouseLeave()
{
	if(!pressed)
	{
		SetStyleColor("background-color", GetStyleColor("bg-default-color"));
	}

	close_highlighted = false;

	Update();
}

bool UpdateButtonBar::OnMouseDown(const Point& pos, InputKey key)
{
	if(key != InputKey::LeftMouse || pressed || close_pressed) return false;

	SetPointerCapture();

	if(pos.x > bar_height)
	{
		SetStyleColor("background-color", GetStyleColor("bg-press-color"));
		pressed = true;
	}
	else
	{
		close_pressed = true;
	}

	Update();

	return false;
}

bool UpdateButtonBar::OnMouseUp(const Point& pos, InputKey key)
{
	if(key != InputKey::LeftMouse) return false;

	ReleasePointerCapture();

	if(pos.y > 0 && pos.y < bar_height && pos.x < GetWidth())
	{
		if(pos.x > bar_height)
		{
			if(pressed)
			{
				//TODO
			}

			SetStyleColor("background-color", GetStyleColor("bg-highlight-color"));
		}
		else if(pos.x > 0 && pos.x < bar_height)
		{
			if(close_pressed)
			{
				//TODO
				Hide();
				//GetLauncher()->UpdateSize();
			}

			SetStyleColor("background-color", GetStyleColor("bg-default-color"));
		}
	}
	else
	{
		SetStyleColor("background-color", GetStyleColor("bg-default-color"));
	}

	pressed = false;
	close_pressed = false;

	Update();

	return false;
}

double UpdateButtonBar::GetPreferredHeight() const
{
	return bar_height;
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
	Show();
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
