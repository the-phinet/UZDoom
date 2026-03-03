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
#include <zwidget/widgets/textlabel/textlabel.h>

class ChoicePopup : public Widget
{
	std::vector<std::unique_ptr<Widget>> cleanup;
	ChoicePopup(Widget * parent, const std::string &title, const std::vector<std::string> &text, const std::vector<std::tuple<std::string, int, std::function<void(ChoicePopup&)>>> &actions)
		: Widget(parent->Window(), WidgetType::Window)
	{
		Size screenSize = GetScreenSize();

		double windowWidth = 500.0;
		double windowHeight = text.size() > 0 ? 90.0 + (20 * text.size()): 80.0;

		SetFrameGeometry((screenSize.width - windowWidth) * 0.5, (screenSize.height - windowHeight) * 0.5, windowWidth, windowHeight);

		SetWindowTitle(title);

		if(text.size() > 0)
		{
			int top = 5;

			for(int i = 0; i < text.size(); i++)
			{
				TextLabel* text_widget = new TextLabel(this);

				text_widget->SetText(text[i]);

				text_widget->SetFrameGeometry(0, top, windowWidth, 20);
				text_widget->SetTextAlignment(TextLabelAlignment::Center);

				cleanup.push_back(std::unique_ptr<Widget>{text_widget});

				top += 20;
			}
		}

		int left = 0;
		int count = 0;

		for(auto &act : actions)
		{
			count++;

			PushButton * btn = new PushButton(this);

			btn->SetText(std::get<0>(act));

			int len = 90 + std::get<1>(act) * 30;

			if(count == actions.size())
			{
				btn->SetFrameGeometry(GetWidth() - (len + 5), GetHeight() - 35, len, 30);
			}
			else
			{
				btn->SetFrameGeometry(left + 5, GetHeight() - 35, len, 30);
			}

			btn->OnClick = [this, act](){std::get<2>(act)(*this);};

			cleanup.push_back(std::unique_ptr<Widget>{btn});

			left += len + 5;
		}

		Show();
		//SetModalCapture(); // doesn't work for windows, but popups don't have window decorations, why???
	}

	void OnClose() override
	{
		currentPopup = nullptr;
	}

	static std::unique_ptr<ChoicePopup> currentPopup;
public:
	static void Open(Widget * parent, const std::string &title, const std::vector<std::string> &text, const std::vector<std::tuple<std::string, int, std::function<void(ChoicePopup&)>>> &actions)
	{
		if(currentPopup)
		{
			currentPopup->Close();
		}

		currentPopup = std::unique_ptr<ChoicePopup>(new ChoicePopup(parent, title, text, actions));
	}
};

std::unique_ptr<ChoicePopup> ChoicePopup::currentPopup;

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

				std::vector<std::string> updateInfo;

				updateInfo.push_back("5.0.1");

				ChoicePopup::Open(this, "Update", updateInfo,
				{
					{"View Release Notes", 4, [](auto &self){
						//self.Close();
					}},
					{"Update", 0, [](auto &self){
						self.Close();
					}}
				});
			}

			SetStyleColor("background-color", GetStyleColor("bg-highlight-color"));
		}
		else if(pos.x > 0 && pos.x < bar_height)
		{
			if(close_pressed)
			{
				//TODO

				ChoicePopup::Open(this, "Dismiss Update?", {},
				{
					{"Dismiss", 0, [this](auto &self){
						this->Hide();
						self.Close();
					}},
					{"Skip Update", 1, [this](auto &self){
						this->Hide();
						self.Close();
					}},
					{"Disable Update Checker", 3, [this](auto &self){
						this->Hide();
						self.Close();
					}}
				});
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
