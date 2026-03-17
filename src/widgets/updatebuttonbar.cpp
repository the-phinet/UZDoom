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
#include "c_cvars.h"
#include "m_misc.h"
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <ctime>

CVAR(String, cached_update, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET);
CVAR(String, skipped_update, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET);
CVAR(String, last_update_check, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET);
CVAR(Int, update_interval, 7, CVAR_ARCHIVE | CVAR_GLOBALCONFIG); // by default, check once per week
CVAR(Bool, auto_updates, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, check_updates, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR(Bool, check_updates_initialized, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)

update_info_t GetUpdateInfo();

class ChoicePopup : public Widget
{
public:
	using ActionListType = std::vector<std::tuple<std::string, int, std::function<void(ChoicePopup&)>>>;
private:
	std::vector<std::unique_ptr<Widget>> cleanup;
	bool allowCloseButton;
	ChoicePopup(Widget * parent, const std::string &title, const std::vector<std::string> &text, const ActionListType &actions, double windowWidth, bool allowClose)
		: Widget(parent->Window(), WidgetType::Utility, RenderAPI::Unspecified, false)
	{
		allowCloseButton = allowClose;

		Size screenSize = GetScreenSize();

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

			btn->OnClick = [this, act]()
			{
				std::get<2>(act)(*this);
			};

			cleanup.push_back(std::unique_ptr<Widget>{btn});

			left += len + 5;
		}

		Show();
		ActivateWindow();
		SetModalCapture(true);
	}

	void OnClose() override
	{
		ReleaseModalCapture(true);
		currentPopup = nullptr;
	}

	static std::unique_ptr<ChoicePopup> currentPopup;

	friend class LauncherWindow;
public:
	static void Open(Widget * parent, const std::string &title, const std::vector<std::string> &text, const ActionListType &actions, double windowWidth = 500.0, bool allowClose = true)
	{
		if(currentPopup)
		{
			currentPopup->Close();
		}

		currentPopup = std::unique_ptr<ChoicePopup>(new ChoicePopup(parent, title, text, actions, windowWidth, allowClose));
	}

	void OnWindowClose()
	{
		if(allowCloseButton) Close();
	}
};

std::unique_ptr<ChoicePopup> ChoicePopup::currentPopup;

constexpr int bar_height = 30;
constexpr int close_margin = 6;
constexpr int arrow_margin = 0;

void LauncherWindow::OnWindowClose()
{ // don't close launcher window if popup is being shown
	if(!ChoicePopup::currentPopup) Close();
}

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
	if(currentUpdate.has_value())
	{
		text.Format("New Update Available: %u.%u.%u", currentUpdate->version.major, currentUpdate->version.minor, currentUpdate->version.revision); // TODO: localize
	}
}

void UpdateButtonBar::OnPaint(Canvas* canvas)
{
	canvas->fillRect(Rect(0, 0, bar_height, bar_height), close_pressed ? GetStyleColor("close-press-color") : close_highlighted ? GetStyleColor("close-highlight-color") : GetStyleColor("bg-default-color"));

	Rect box = canvas->measureText(text.GetChars());
	canvas->drawText(Point((GetWidth() - bar_height - box.width) * 0.5, (bar_height * 0.5) + (box.height * 0.35)), GetStyleColor("color"), text.GetChars());

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

void OpenUpdateMenu(UpdateButtonBar * self, bool isAutoUpdate);

void OpenDismissUpdateMenu(UpdateButtonBar * self, bool isAutoUpdate)
{
	ChoicePopup::ActionListType actions = { // TODO: localize
		{"Dismiss", 0, [oldSelf = self, isAutoUpdate](auto &self){
			oldSelf->Hide();
			self.Close();
		}},
		{"Skip Update", 1, [oldSelf = self](auto &self){
			skipped_update = FString(oldSelf->currentUpdate->version);
			M_SaveDefaults(NULL); // save settings
			oldSelf->Hide();
			self.Close();
		}},
		{"Disable Update Checker", 3, [oldSelf = self](auto &self){
			check_updates = false;
			M_SaveDefaults(NULL); // save settings
			oldSelf->Hide();
			self.Close();
		}}
	};

	if(isAutoUpdate)
	{
		actions.push_back({"Back", 0, [oldSelf = self](auto &self){
			OpenUpdateMenu(oldSelf, true);
		}});
	}

	ChoicePopup::Open(self, "Dismiss Update?", {}, actions, 550.0, !isAutoUpdate); // TODO: localize
}

void OpenUpdateMenu(UpdateButtonBar * self, bool isAutoUpdate)
{
	ChoicePopup::ActionListType actions = { // TODO: localize
		{"View Release Notes", 4, [isAutoUpdate, oldSelf = self](auto &self){
			ChoicePopup::Open(oldSelf, "Release Notes", oldSelf->currentUpdate->release_notes, // TODO: localize
			{
				{"Back", 0, [isAutoUpdate, oldSelf](auto &self){
					OpenUpdateMenu(oldSelf, isAutoUpdate);
				}}
			});
		}},
		{"Update", 0, [](auto &self){
			//TODO
			self.Close();
		}}
	};

	if(isAutoUpdate)
	{
		actions.push_back({"Dismiss", 0, [oldSelf = self](auto &self){
			OpenDismissUpdateMenu(oldSelf, true);
		}});
	}

	if(self->currentUpdate->cached)
	{
		self->currentUpdate = GetUpdateInfo(); // we only have the cached update number right now, grab full update info
	}

	std::vector<std::string> updateInfo;

	updateInfo.push_back((GAMENAME + (" " + FString(self->currentUpdate->version))).GetChars());

	ChoicePopup::Open(self, isAutoUpdate ? "New Update Available" : "Update", updateInfo, actions, 500.0, !isAutoUpdate); // TODO: localize
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
				OpenUpdateMenu(this, false);
			}

			SetStyleColor("background-color", GetStyleColor("bg-highlight-color"));
		}
		else if(pos.x > 0 && pos.x < bar_height)
		{
			if(close_pressed)
			{
				//TODO

				ChoicePopup::Open(this, "Dismiss Update?", {}, // TODO: localize
				{
					{"Dismiss", 0, [this](auto &self){
						this->Hide();
						self.Close();
					}},
					{"Skip Update", 1, [this](auto &self){
						skipped_update = FString(currentUpdate->version);
						M_SaveDefaults(NULL); // save settings
						this->Hide();
						self.Close();
					}},
					{"Disable Update Checker", 3, [this](auto &self){
						check_updates = false;
						M_SaveDefaults(NULL); // save settings
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

static void OpenUpdateIntervalChoice(Widget * parent);

static void OpenUpdateInitChoice(Widget * parent)
{
	ChoicePopup::Open(parent, "Update Checker", {"Would you like to automatically check for updates?", "(this can be changed later in the options tab)"}, // TODO: localize
	{
		{
			"Yes, and prompt to install updates", 5, [](auto &self)
			{
				auto_updates = true;
				OpenUpdateIntervalChoice(self.Parent());
			}
		},{
			"Yes", 5, [](auto &self)
			{
				auto_updates = false;
				OpenUpdateIntervalChoice(self.Parent());
			}
		},{
			"No", 0, [](auto &self)
			{
				check_updates = false;
				check_updates_initialized = true;
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		}
	}, 600.0, false);
}

static date_t getDate();

static void OpenUpdateIntervalChoice(Widget * parent)
{
	ChoicePopup::Open(parent, "Update Checker", {"How often would you like to check for updates?", "(this can be changed later in the options tab)"}, // TODO: localize
	{
		{
			"Every other day", 2, [](auto &self)
			{
				check_updates = true;
				update_interval = 2;
				check_updates_initialized = true;
				last_update_check = FString(getDate());
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Every week", 1, [](auto &self)
			{
				check_updates = true;
				update_interval = 7;
				check_updates_initialized = true;
				last_update_check = FString(getDate() - 5); // first check always in 2 days
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Every month", 1, [](auto &self)
			{
				check_updates = true;
				update_interval = 30;
				check_updates_initialized = true;
				last_update_check = FString(getDate() - 28); // first check always in 2 days
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Back", 0, [](auto &self)
			{
				auto_updates = false;
				OpenUpdateInitChoice(self.Parent());
			}
		}
	}, 550.0, false);
}

static date_t getDate()
{
	time_t t;
	time(&t);
	struct tm curTime;
#if defined(_MSC_VER) || defined(MINGW_HAS_SECURE_API)
	//use microsoft's botched localtime_s
	localtime_s(&curTime, &t);
#elif defined(__unix__) || defined(__APPLE__) || defined(_POSIX_VERSION) || __cplusplus >= 202302L
	localtime_r(&t, &curTime);
#elif defined(__STDC_LIB_EXT1__)
	//use the actual standard localtime_s
	localtime_s(&t, &curTime);
#else
	struct tm* tm_ptr = localtime(&t);
	if (tm_ptr) curTime = *tm_ptr;
	else curTime = {};
#endif
	return {curTime.tm_mday, curTime.tm_mon + 1, curTime.tm_year + 1900};
}

static date_t parseDate(FString str, date_t fallback) // parse "day-month-year" string into tm, returns current date on fail
{
	auto sections = str.Split("-");

	if(sections.size() != 3 || !sections[0].IsInt()|| !sections[1].IsInt()|| !sections[2].IsInt()) return fallback; // invalid string

	int year = sections[0].ToLong();
	int month = sections[1].ToLong();
	int day = sections[2].ToLong();

	// don't validate year
	if(month < 1 || month > 12 || day < 1 || day >= (date_t::dayCount(year, month))) return fallback; // invalid date

	return {day, month, year};
}

update_info_t GetUpdateInfo()
{
	//DO CHECK
	return update_info_t{{5, 0, 1}, false, {"test", "lol", "test"}};
}

bool isVersionInvalid(VersionInfo ver)
{
	return ver.major == USHRT_MAX || ver.minor == USHRT_MAX || ver.revision == USHRT_MAX || ver == GetCurrentVersion();
}

void UpdateButtonBar::CheckForUpdate()
{
	if(!check_updates_initialized)
	{
		OpenUpdateInitChoice(this);
	}
	else
	{
		if(cached_update->Length() > 0)
		{
			VersionInfo cachedVer(cached_update);

			if(isVersionInvalid(cachedVer))
			{
				cached_update = "";
				M_SaveDefaults(NULL); // save settings
			}
			else
			{
				currentUpdate = update_info_t{cachedVer, true, {}};
			}
		}

		VersionInfo skippedVer;

		if(skipped_update->Length() > 0)
		{
			VersionInfo skippedVerTmp = VersionInfo((const char *)skipped_update);
			if(isVersionInvalid(skippedVerTmp))
			{
				skipped_update = "";
				M_SaveDefaults(NULL); // save settings
			}
			else
			{
				skippedVer = skippedVerTmp;
			}
		}

		auto curTime = getDate();
		auto nextCheckTime = parseDate((FString)last_update_check, curTime - (update_interval + 1)) + update_interval;

		if(curTime >= nextCheckTime || currentUpdate.has_value())
		{
			if(!currentUpdate.has_value() || curTime >= nextCheckTime) // invalidate cache if check time is due
			{
				currentUpdate = GetUpdateInfo();
				last_update_check = FString(getDate());
				if(currentUpdate.has_value())
				{
					cached_update = FString(currentUpdate->version);
				}
				M_SaveDefaults(NULL); // save settings
			}

			if(currentUpdate.has_value())
			{
				if(currentUpdate->version > GetCurrentVersion() && (skippedVer != currentUpdate->version))
				{
					if(auto_updates)
					{
						OpenUpdateMenu(this, true);
					}
					else
					{
						UpdateLanguage();
						Show();
					}
				}
			}
		}
	}
}
