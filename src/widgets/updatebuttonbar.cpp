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

//ugh something is including windows.h, probably curl? disable parts of it to minimize conflicts
#ifndef NOMINMAX
	#define NOMINMAX // mingw already defines NOMINMAX??????
#endif

#define WIN32_LEAN_AND_MEAN

// The #defines here *MUST* match serializer.cpp, or we will get countless strange errors.
#define RAPIDJSON_48BITPOINTER_OPTIMIZATION 0	// disable this insanity which is bound to make the code break over time.
#define RAPIDJSON_HAS_CXX11_RVALUE_REFS 1
#define RAPIDJSON_HAS_CXX11_RANGE_FOR 1
#define RAPIDJSON_PARSE_DEFAULT_FLAGS kParseFullPrecisionFlag

#include <miniz.h>
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"

#include "updatebuttonbar.h"
#include "launcherwindow.h"
#include "gstrings.h"
#include "c_cvars.h"
#include "m_misc.h"
#include "i_net.h"
#include "engineerrors.h"
#include "widgets/themedata.h"
#include <zwidget/widgets/pushbutton/pushbutton.h>
#include <zwidget/widgets/textlabel/textlabel.h>
#include <ctime>
#include <curl/curl.h>
#include <algorithm>
#include <format>
#include <thread>
#include <mutex>
#include <filesystem>
#include "filesystem.h"
#include "cmdlib.h"
#ifdef _WIN32
	#include <shellapi.h>
#endif

CVAR(String, updater_cached_update, "", CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);
CVAR(String, updater_skipped_update, "", CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);
CVAR(String, updater_last_update_check, "", CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);
CVAR(Bool, updater_check_updates_initialized, false, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);
CVAR(Int, updater_update_interval, 7, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG); // by default, check once per week
CVAR(Bool, updater_auto_updates, false, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG);
CVAR(Bool, updater_check_updates, false, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG);
CVAR(Bool, updater_debug_always_update, false, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);
CVAR(Bool, updater_debug_throttle_download, false, CVAR_ARCHIVE | CVAR_CONFIG_ONLY | CVAR_GLOBALCONFIG | CVAR_NOSET | CVAR_HIDDEN);

static std::vector<std::string> SplitNewLines(const char * str, size_t len)
{
	TArray<FString> s = FString(str, len).SplitNewLines(60, 70);
	std::vector<std::string> ret(s.size());

	for(int i = 0; i < s.size(); i++)
	{
		ret[i] = std::string(s[i].GetChars(), s[i].Len());
	}

	return ret;
}

static std::vector<std::string> SplitNewLines(std::string str)
{
	return SplitNewLines(str.c_str(), str.length());
}

class PopupBase;

std::unique_ptr<PopupBase> currentPopup;

class PopupBase : public Widget
{ // TODO move popups to their own header/file so that more things can use them, and change it so that multiple popups can open and stack, instead of just overwriting one another
public:
	using ActionListType = std::vector<std::tuple<std::string, int, std::function<void(PopupBase&)>>>;
	virtual ~PopupBase() = default;

protected:
	std::vector<std::unique_ptr<Widget>> cleanup;
	bool allowCloseButton;

	using Widget::Widget;

	double windowHeight;
	double windowWidth;

	virtual void OnClose() override
	{
		ReleaseModalCapture(true);
		currentPopup = nullptr;
	}

	virtual void OnWindowClose() override
	{
		if(allowCloseButton) Close();
	}

public:
	static ActionListType ConcatActions(const ActionListType &a, const ActionListType &b)
	{
		ActionListType tmp;
		tmp.insert(tmp.end(), a.begin(), a.end());
		tmp.insert(tmp.end(), b.begin(), b.end());
		return tmp;
	}
	static std::vector<std::string> ConcatText(const std::vector<std::string> &a, const std::vector<std::string> &b)
	{
		std::vector<std::string> tmp;
		tmp.insert(tmp.end(), a.begin(), a.end());
		tmp.insert(tmp.end(), b.begin(), b.end());
		return tmp;
	}
};


template<class Derived>
class ChoicePopup : public PopupBase
{
public:
	using ActionListType = std::vector<std::tuple<std::string, int, std::function<void(ChoicePopup&)>>>;

protected:
	int GetExtraHeight() { return 0; }

	void AddExtraElements(int top) {}

	std::vector<TextLabel *> text;
public:
	ChoicePopup(Widget * parent, const std::string &title, const std::vector<std::string> &text, const PopupBase::ActionListType &actions, double _windowWidth, bool allowClose)
		: PopupBase(parent->Window(), WidgetType::Utility, RenderAPI::Unspecified, false)
	{
		allowCloseButton = allowClose;

		Size screenSize = GetScreenSize();

		int extraHeight = static_cast<Derived*>(this)->GetExtraHeight();

		windowWidth = _windowWidth;

		windowHeight = text.size() > 0 ? 90.0 + (20 * text.size()): 80.0;

		if(actions.size() == 0)
		{
			windowHeight -= 40;
		}

		if(extraHeight > 0)
		{
			windowHeight += extraHeight + 10;
		}

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
				this->text.push_back(text_widget);

				top += 20;
			}

			static_cast<Derived*>(this)->AddExtraElements(top + 5);
		}
		else
		{
			static_cast<Derived*>(this)->AddExtraElements(5);
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
private:
	static std::unique_ptr<PopupBase> currentPopup;

	friend class LauncherWindow;
};

class BasicPopup : public ChoicePopup<BasicPopup>
{
public:
	using ChoicePopup::ChoicePopup;
};

template<class T = BasicPopup>
T& OpenPopup(Widget * parent, const std::string &title, const std::vector<std::string> &text = {}, const PopupBase::ActionListType &actions = {}, double windowWidth = 500.0, bool allowClose = true)
{
	if(currentPopup)
	{
		currentPopup->Close();
	}

	currentPopup = std::unique_ptr<PopupBase>(new T(parent, title, text, actions, windowWidth, allowClose));

	return *static_cast<T*>(currentPopup.get());
}

template<typename T>
class ProgressPopup : public ChoicePopup<T>
{
private:
	Widget * updateBar;
	double updateBarX;
	double updateBarY;
	double updateBarWidth;
	double updateBarHeight;
protected:

	double updateBarPercentage;

	template<class>
	friend void OpenPopup(Widget *, const std::string &, const std::vector<std::string> &, const PopupBase::ActionListType &, double, bool);

	int GetExtraHeight()
	{
		return 30;
	}

	void AddExtraElements(int top)
	{
		double updateBarBackgroundHeight = 30;
		double updateBarBackgroundWidth = Widget::GetWidth() - 10;

		updateBarX = 6;
		updateBarY = top + 1;
		updateBarWidth = updateBarBackgroundWidth - 2;
		updateBarHeight = updateBarBackgroundHeight - 2;
		updateBarPercentage = 0.75;

		//border
		Widget * updateBarBackground = new Widget(this);

		updateBarBackground->SetStyleColor("background-color", Theme::getMain(0.9f));
		updateBarBackground->SetFrameGeometry(5, top, updateBarBackgroundWidth, updateBarBackgroundHeight);

		PopupBase::cleanup.push_back(std::unique_ptr<Widget>{updateBarBackground});

		//background
		updateBarBackground = new Widget(this);

		updateBarBackground->SetStyleColor("background-color", Colorf(0.0f, 0.3f, 0.5f, 1.0f));
		updateBarBackground->SetFrameGeometry(updateBarX, updateBarY, updateBarWidth, updateBarHeight);

		PopupBase::cleanup.push_back(std::unique_ptr<Widget>{updateBarBackground});

		//bar
		updateBar = new Widget(this);

		updateBar->SetStyleColor("background-color", Colorf(0.2f, 0.5f, 0.75f, 1.0f));
		RefreshBar();

		PopupBase::cleanup.push_back(std::unique_ptr<Widget>{updateBar});

		top += 30;
	}

	void RefreshBar()
	{
		updateBar->SetFrameGeometry(updateBarX, updateBarY, updateBarWidth * updateBarPercentage, updateBarHeight);
	}
public:

	friend class ChoicePopup<T>;
	using ChoicePopup<T>::ChoicePopup;
};

constexpr int bar_height = 30;
constexpr int close_margin = 6;
constexpr int arrow_margin = 0;

void LauncherWindow::OnWindowClose()
{ // don't close launcher window if popup is being shown
	if(!currentPopup) Close();
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

static FString UpdateToString(VersionInfo update)
{
	FString str = "";

	switch(CURRENT_UPDATE_CHANNEL)
	{
	case UpdateChannel::STABLE:
		str.Format("%u.%u.%u", update.major, update.minor, update.revision);
		break;
	case UpdateChannel::PREVIEW:
		str.Format("%u.%u.%u-pre-%u", update.major, update.minor, update.revision, update.distance);
		break;
	case UpdateChannel::TESTING:
		str.Format("%u.%u.%u-pre-%u (experimental)", update.major, update.minor, update.revision, update.distance); // TODO: localize
		break;
	case UpdateChannel::RELEASE_CANDIDATE:
		if(update.distance != 0 && update.distance != RC_REVISION_NOTRC)
		{
			str.Format("%u.%u.%u-rc%u", update.major, update.minor, update.revision, update.distance);
		}
		else
		{
			str.Format("%u.%u.%u", update.major, update.minor, update.revision);
		}
		break;
	}
	return str;
}

void UpdateButtonBar::UpdateLanguage()
{
	if(currentUpdate.has_value())
	{
		text = "New Update Available: " + UpdateToString(currentUpdate->version); // TODO: localize
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

void OpenDismissUpdateMenu(UpdateButtonBar * buttonBar, bool isAutoUpdate)
{ // TODO move to method of UpdateButtonBar
	PopupBase::ActionListType actions = {
		{"Dismiss", 0, [=](auto &self){ // TODO: localize
			buttonBar->Hide();
			self.Close();
		}},
		{"Skip Update", 1, [=, this](auto &self){ // TODO: localize
			updater_skipped_update = FString(currentUpdate->version);
			M_SaveDefaults(NULL); // save settings
			buttonBar->Hide();
			self.Close();
		}},
		{"Disable Update Checker", 3, [=, this](auto &self){ // TODO: localize
			updater_check_updates = false;
			M_SaveDefaults(NULL); // save settings
			buttonBar->Hide();
			self.Close();
		}}
	};

	if(isAutoUpdate)
	{
		actions.push_back({"Back", 0, [=](auto &self){ // TODO: localize
			buttonBar->OpenUpdateMenu(true);
		}});
	}

	OpenPopup(buttonBar, "Dismiss Update?", {}, actions, 550.0, !isAutoUpdate); // TODO: localize
}

void OpenFailedUpdateMenu(UpdateButtonBar * buttonBar, const std::string &err, bool checker)
{ // TODO move to method of UpdateButtonBar
	PopupBase::ActionListType actions = {
		{"Dismiss", 0, [=](auto &self){ // TODO: localize
			buttonBar->Hide();
			self.Close();
		}},
		{"Disable Update Checker", 3, [=, this](auto &self){ // TODO: localize
			updater_check_updates = false;
			M_SaveDefaults(NULL); // save settings
			buttonBar->Hide();
			self.Close();
		}}
	};

	OpenPopup(buttonBar, checker ? "Checking for Update Failed" : "Update Failed", SplitNewLines((checker ? "Checking for Update Failed\n" : "Update Failed\n") + err), actions, 550.0, false); // TODO: localize
}
static void StartUpdate(UpdateButtonBar * buttonBar);

void UpdateButtonBar::OpenUpdateMenu(bool isAutoUpdate)
{
	PopupBase::ActionListType actions = {
		{"View Release Notes", 4, [this, isAutoUpdate](auto &self){
			OpenPopup(this, "Release Notes", this->currentUpdate->release_notes, // TODO: localize
			{
				{"Back", 0, [this, isAutoUpdate](auto &self){ // TODO: localize
					this->OpenUpdateMenu(isAutoUpdate);
				}}
			});
		}},
		{"Update", 0, [this](auto &currentPopup){ // TODO: localize
			auto temp = this;
			currentPopup.Close();
			StartUpdate(temp);
		}}
	};

	if(isAutoUpdate)
	{
		actions.push_back({"Dismiss", 0, [this](auto &self){ // TODO: localize
			OpenDismissUpdateMenu(this, true);
		}});
	}

	if(currentUpdate->cached)
	{
		bool ok = false;
		currentUpdate = GetUpdateInfo(ok); // we only have the cached update number right now, grab full update info
		if(!ok || !currentUpdate.has_value()) return;
		updater_cached_update = FString(currentUpdate->version);
		updater_last_update_check = std::to_string(getCurrentDate()).c_str();
		M_SaveDefaults(NULL); // save settings
		UpdateLanguage();
	}

	std::vector<std::string> updateInfo;

	updateInfo.push_back((GAMENAME + (" " + UpdateToString(currentUpdate->version))).GetChars());

	OpenPopup(this, isAutoUpdate ? "New Update Available" : "Update", updateInfo, actions, 500.0, !isAutoUpdate); // TODO: localize
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
				OpenUpdateMenu(false);
			}

			SetStyleColor("background-color", GetStyleColor("bg-highlight-color"));
		}
		else if(pos.x > 0 && pos.x < bar_height)
		{
			if(close_pressed)
			{
				//TODO

				OpenPopup(this, "Dismiss Update?", {}, // TODO: localize
				{
					{"Dismiss", 0, [this](auto &self){ // TODO: localize
						this->Hide();
						self.Close();
					}},
					{"Skip Update", 1, [this](auto &self){ // TODO: localize
						updater_skipped_update = FString(currentUpdate->version);
						M_SaveDefaults(NULL); // save settings
						this->Hide();
						self.Close();
					}},
					{"Disable Update Checker", 3, [this](auto &self){ // TODO: localize
						updater_check_updates = false;
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
	OpenUpdateMenu(false);
}

LauncherWindow* UpdateButtonBar::GetLauncher() const
{
	return static_cast<LauncherWindow*>(Parent());
}

static void OpenUpdateIntervalChoice(Widget * parent);

static void OpenUpdateInitChoice(Widget * parent)
{
	OpenPopup(parent, "Update Checker", {"Would you like to automatically check for updates?", "(this can be changed later in the options tab)"}, // TODO: localize
	{
		{
			"Yes, and prompt to install updates", 5, [](auto &self) // TODO: localize
			{
				updater_auto_updates = true;
				OpenUpdateIntervalChoice();
			}
		},{
			"Yes", 5, [](auto &self) // TODO: localize
			{
				updater_auto_updates = false;
				OpenUpdateIntervalChoice();
			}
		},{
			"No", 0, [](auto &self) // TODO: localize
			{
				updater_check_updates = false;
				updater_check_updates_initialized = true;
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		}
	}, 600.0, false);
}

static date_t getDate();

static void OpenUpdateIntervalChoice(Widget * parent)
{
	OpenPopup(parent, "Update Checker", {"How often would you like to check for updates?", "(this can be changed later in the options tab)"}, // TODO: localize
	{
		{
			"Every other day", 2, [](auto &self) // TODO: localize
			{
				updater_check_updates = true;
				updater_update_interval = 2;
				updater_check_updates_initialized = true;
				updater_last_update_check = FString(date_t::getCurrentDate());
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Every week", 1, [](auto &self) // TODO: localize
			{
				updater_check_updates = true;
				updater_update_interval = 7;
				updater_check_updates_initialized = true;
				updater_last_update_check = FString(date_t::getCurrentDate() - 5); // first check always in 2 days
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Every month", 1, [](auto &self) // TODO: localize
			{
				updater_check_updates = true;
				updater_update_interval = 30;
				updater_check_updates_initialized = true;
				updater_last_update_check = FString(date_t::getCurrentDate() - 28); // first check always in 2 days
				M_SaveDefaults(NULL); // save settings
				self.Close();
			}
		},{
			"Back", 0, [](auto &self) // TODO: localize
			{
				updater_auto_updates = false;
				OpenUpdateInitChoice();
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

	int year = (int)sections[0].ToLong();
	int month = (int)sections[1].ToLong();
	int day = (int)sections[2].ToLong();

	// don't validate year
	if(month < 1 || month > 12 || day < 1 || day >= (date_t::dayCount(year, month))) return fallback; // invalid date

	return {day, month, year};
}


bool curl_initialized = false;
bool curl_initialized_ok = false;

static bool InitCurl(UpdateButtonBar * buttonBar)
{
	if(curl_initialized) return curl_initialized_ok;
	if(!IsNetworkStartedLean()) StartNetworkLean();

	if(!curl_initialized)
	{
		CURLcode ret;

		curl_initialized = true;
		curl_initialized_ok = ((ret = curl_global_init(CURL_GLOBAL_DEFAULT)) == CURLE_OK);

		if(!curl_initialized_ok)
		{
			OpenFailedUpdateMenu(buttonBar, "curl_global_init failed: "+std::string(curl_easy_strerror(ret)), true);
		}
	}

	return curl_initialized_ok;
}


static size_t callAcceptData(void *buf, size_t sz, size_t num, void *self);
static size_t callAcceptHeader(void *buf, size_t sz, size_t num, void *self);
static int callUpdateProgress(void * self, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

class CurlEasy
{
	friend size_t callAcceptData(void *buf, size_t sz, size_t num, void *self);
	friend size_t callAcceptHeader(void *buf, size_t sz, size_t num, void *self);
	friend int callUpdateProgress(void * self, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

	CURL * curl;

	std::atomic<bool> cancelled = false;

	virtual void UpdateProgress(curl_off_t dltotal, curl_off_t dlnow) {}; //, curl_off_t ultotal, curl_off_t ulnow) {};
	virtual void AcceptHeader(TArrayView<std::byte> data) {};
	virtual void AcceptData(TArrayView<std::byte> data) = 0;
	virtual void OnError(const char * err) {};

protected:

	void CancelDownload()
	{
		cancelled = true;
	}

	CurlEasy(const char * userAgent, bool useProgress, bool readHeader)
	{
		curl = curl_easy_init();

		if(!curl) I_Error("curl_easy_init failed");

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callAcceptData);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);

		if(readHeader)
		{
			curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, callAcceptHeader);
			curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
		}

		if(useProgress)
		{
			curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, callUpdateProgress);
			curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
			curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		}

		curl_easy_setopt(curl,CURLOPT_USERAGENT, userAgent);

		curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA); // use native SSL CA

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // allow redirects

		if(updater_debug_throttle_download)
		{
			curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, 1000000L); // set max speed of ~1mb/s for testing
		}
	}

	bool Perform(const std::string &url, const std::string &acceptEncoding, bool * cancelled = nullptr)
	{
		if(!curl) return false;

		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, acceptEncoding.c_str());

		if(CURLcode ret = curl_easy_perform(curl); ret != CURLE_OK)
		{
			if(cancelled) (*cancelled) = this->cancelled;
			OnError(curl_easy_strerror(ret));
			return false;
		}
		if(cancelled) (*cancelled) = this->cancelled;
		return true;
	}
public:

	virtual ~CurlEasy()
	{
		Close();
	}

	bool IsOpen()
	{
		return curl != nullptr;
	}

	void Close()
	{
		if(curl)
		{
			curl_easy_cleanup(curl);
			curl = nullptr;
		}
	}
};

static size_t callAcceptData(void *buf, size_t sz, size_t num, void *self)
{
	assert(sz == 1); // according to curl docs, size is always 1, but doesn't hurt validating it with an assert
	((CurlEasy*)self)->AcceptData({(std::byte*)buf, (unsigned)num});
	return num;
}

static size_t callAcceptHeader(void *buf, size_t sz, size_t num, void *self)
{
	assert(sz == 1); // according to curl docs, size is always 1, but doesn't hurt validating it with an assert
	((CurlEasy*)self)->AcceptHeader({(std::byte*)buf, (unsigned)num});
	return num;
}

static int callUpdateProgress(void * self, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	((CurlEasy*)self)->UpdateProgress(dltotal, dlnow); //, ultotal, ulnow);
	return ((CurlEasy*)self)->cancelled;
}

const char * getMainURL(UpdateChannel channel)
{
	switch(channel)
	{
	case UpdateChannel::STABLE:
		return UPDATER_URL_STABLE;
	case UpdateChannel::PREVIEW:
		return UPDATER_URL_PREVIEW;
	case UpdateChannel::TESTING:
		return UPDATER_URL_TESTING;
	case UpdateChannel::RELEASE_CANDIDATE:
		return UPDATER_URL_ALL;
	default:
		I_Error("Unknown Update Channel");
	}
}

const char * getBackupURL(UpdateChannel channel)
{
	switch(channel)
	{
	case UpdateChannel::STABLE:
		return UPDATER_URL_STABLE_BACKUP;
	case UpdateChannel::PREVIEW:
		return UPDATER_URL_PREVIEW_BACKUP;
	case UpdateChannel::TESTING:
		return UPDATER_URL_TESTING_BACKUP;
	case UpdateChannel::RELEASE_CANDIDATE:
		return UPDATER_URL_ALL_BACKUP;
	default:
		I_Error("Unknown Update Channel");
	}
}

class UpdateChecker : public CurlEasy
{
	std::string buffer;

	virtual void AcceptData(TArrayView<std::byte> data) override
	{
		buffer += std::string((const char *)data.Data(), data.Size());
	}

	const char * firstErr = nullptr;

	virtual void OnError(const char * err)
	{
		if(!firstErr) firstErr = err;
	};
public:
	UpdateChecker() : CurlEasy(GAMENAME " Updater", false, false)
	{}

	std::optional<rapidjson::Document> Perform(UpdateButtonBar * buttonBar, UpdateChannel channel)
	{
		if(!CurlEasy::Perform(getMainURL(channel), "application/vnd.github+json"))
		{
			buffer = "";
			if(!CurlEasy::Perform(getBackupURL(channel), "application/vnd.github+json"))
			{
				OpenFailedUpdateMenu(buttonBar, firstErr, true);
				return std::nullopt;
			}
		}
		Close();

		rapidjson::Document doc;
		rapidjson::ParseResult ok = doc.Parse(buffer.c_str(), buffer.length());

		if(!ok)
		{
			OpenFailedUpdateMenu(buttonBar, "Invalid Update JSON", true); // TODO: localize
			return std::nullopt;
		}

		return doc;
	}
};

class JsonDownloader : public CurlEasy
{
	std::string buffer;

	virtual void AcceptData(TArrayView<std::byte> data) override
	{
		buffer += std::string((const char *)data.Data(), data.Size());
	}

	const char * firstErr = nullptr;

	virtual void OnError(const char * err) override
	{
		if(!firstErr) firstErr = err;
	};
public:
	JsonDownloader() : CurlEasy(GAMENAME " Updater", false, false)
	{}

	std::optional<rapidjson::Document> Perform(UpdateButtonBar * buttonBar, const std::string &url)
	{
		if(!CurlEasy::Perform(url, "application/json"))
		{
			OpenFailedUpdateMenu(buttonBar, firstErr, true);
			return std::nullopt;
		}
		Close();

		rapidjson::Document doc;
		rapidjson::ParseResult ok = doc.Parse(buffer.c_str(), buffer.length());

		if(!ok)
		{
			OpenFailedUpdateMenu(buttonBar, "Invalid Update JSON", true); // TODO: localize
			return std::nullopt;
		}

		return doc;
	}
};

void CloseWidgetResources();

class ProgressDownloader : public CurlEasy, public ProgressPopup<ProgressDownloader>
{
	std::vector<std::byte> buffer;

	static inline const std::string names[4] = {
		"B",  // 0
		"KB", // 1
		"MB", // 2
		"GB", // 3
	};

	std::string shortenByteSize(uint64_t size)
	{
		constexpr int maxStep = 3;

		int steps = 0;
		double remainder = 0;

		while(size > 1024 && steps < maxStep)
		{
			remainder = (double)(size % 1024);
			size /= 1024;
			steps++;
		}

		return std::format("{:.3} {}", size + (remainder/1024), names[steps]);
	}
protected:

	virtual void AcceptData(TArrayView<std::byte> data) override
	{
		size_t oldsize = buffer.size();
		buffer.resize(oldsize + data.Size());
		memcpy(buffer.data() + oldsize, data.Data(), data.Size());
	}


	std::mutex progress_lock;
	uint64_t current_download = 0;
	uint64_t total_download = 0;
	bool close_requested = false;
	std::mutex finished_lock;
	bool finished = false;
	bool success = false;

	virtual void UpdateProgress(curl_off_t dltotal, curl_off_t dlnow)
	{
		progress_lock.lock();
		current_download = dlnow;
		total_download = dltotal;
		progress_lock.unlock();
		ProgressPopup::Update();	//FIXME is this thread-safe on SDL2?
									// (not much of a concern for now, since on windows it just
									// calls InvalidateRect, which _is_ thread-safe, but will need
									// looking into once the updater is made cross-platform)

		//TODO calculate average/current download speeds
	};

	virtual void OnPaint(Canvas* canvas) override
	{
		progress_lock.lock();
		updateBarPercentage = std::min(1.0, std::max(0.0, ((double)current_download) / ((double)total_download)));
		text[0]->SetText( std::format("Downloading, {} / {}", shortenByteSize(current_download), shortenByteSize(total_download))); // TODO: localize
		progress_lock.unlock();
		ProgressPopup::RefreshBar();
	}

	const char * err = nullptr;

	virtual void OnError(const char * err) override
	{
		this->err = err;
	};

public:
	ProgressDownloader(Widget * parent, const std::string &title, const std::vector<std::string> &text, const PopupBase::ActionListType &actions, double _windowWidth, bool allowClose)
		:	CurlEasy(GAMENAME " Updater", true, false),
		ProgressPopup(parent, title, ConcatText({"Starting Download..."}, text), actions, _windowWidth, allowClose) // TODO: localize
	{}

	static void DownloaderThread(ProgressDownloader * self, const std::string &url)
	{
		if(!self->CurlEasy::Perform(url, "application/octet-stream", &self->close_requested))
		{
			self->finished_lock.lock();
			self->finished = true;
			self->success = false;
			self->finished_lock.unlock();
		}
		else
		{
			self->finished_lock.lock();
			self->finished = true;
			self->success = true;
			self->finished_lock.unlock();
		}

		self->CurlEasy::Close();

		if(!self->close_requested) self->NotifyWindow();
	}

	std::unique_ptr<std::thread> downloader_thread;

	UpdateButtonBar * buttonBar;

	void Perform(UpdateButtonBar * buttonBar, const std::string &url)
	{
		this->buttonBar = buttonBar;
		downloader_thread = std::make_unique<std::thread>(DownloaderThread, this, url);
	}

	virtual void OnWindowNotified() override
	{
		if(downloader_thread)
		{
			downloader_thread->join();
			downloader_thread = nullptr;
		}

		if(finished && !success)
		{
			OpenFailedUpdateMenu(buttonBar, err, false);
		}
		else
		{
			std::unique_ptr<FResourceFile> zip (FResourceFile::OpenResourceFileMemory(buttonBar->GetDownloadURL().c_str(), (void*)buffer.data(), buffer.size(), true));

			if(!zip)
			{
				OpenPopup(buttonBar, "Failed to open zip file", {"Update was cancelled"}, // TODO: localize
				{
					{"Back", 0, [](auto &self){
						self.Close();
					}}
				});
			}
			else
			{
				#ifdef _WIN32	// this is technically 'portable' code since it only uses the stdlib, but what it does only makes sense on windows,
								// hence the ifdef - linux would need to either use the appimage updater library that i forgot the name of,
								// or just nothing, as it would otherwise be handled by an external package manager (ex. apt/pacman/flatpak/etc)
								// and on macOS i genuinely have no idea what would be needed for an auto-updater

				std::string progdir(::progdir.GetChars());

				try
				{
					if(std::filesystem::exists(progdir + "update"))
					{
						if(std::filesystem::is_directory(progdir + "update"))
						{
							std::filesystem::remove_all(progdir + "update");
							std::filesystem::create_directory(progdir + "update");
						}
						else
						{
							OpenFailedUpdateMenu(buttonBar, "'"+progdir + "update' is not a directory", false); // TODO: localize
							return;
						}
					}
					else
					{
						std::filesystem::create_directory(progdir + "update");
					}

					if(std::filesystem::exists(progdir + "update_backup"))
					{
						if(std::filesystem::is_directory(progdir + "update_backup"))
						{
							std::filesystem::remove_all(progdir + "update_backup");
							std::filesystem::create_directory(progdir + "update_backup");
						}
						else
						{
							OpenFailedUpdateMenu(buttonBar, "'"+progdir + "update_backup' is not a directory", false); // TODO: localize
							return;
						}
					}
					else
					{
						std::filesystem::create_directory(progdir + "update_backup");
					}

				}
				catch(std::exception &e)
				{
					OpenFailedUpdateMenu(buttonBar, e.what(), false);
					return;
				}

				uint32_t n = zip->EntryCountU();

				try
				{
					//extract zip contents into `$PROGDIR/update/`, copy existing files with same name into `$PROGDIR/update_backup/`
					for(uint32_t i = 0; i < n; i++)
					{
						std::string path = zip->getName(i);

						bool isupdaterexe = (path == "updater.exe");

						std::filesystem::path p (path);

						if(p.has_parent_path() && !isupdaterexe)
						{
							std::filesystem::create_directories(progdir + "update/" + p.parent_path().string());
						}

						if(std::filesystem::exists(progdir + path))
						{ // make backup of file, so that a failed update can be reverted
							if(p.has_parent_path())
							{
								std::filesystem::create_directories(progdir + "update_backup/" + p.parent_path().string());
							}
							std::filesystem::copy(progdir + path, progdir + "update_backup/" + path);
						}

						std::string newpath = isupdaterexe ? (progdir + path) : (progdir + "update/" + path); // updater.exe is replaced directly, doesn't go into the update subfolder

						FILE * f = fopen(newpath.c_str(), "wb");
						if(!f)
						{
							std::string err = strerror(errno);
							try
							{
								std::filesystem::remove_all(progdir + "update");
								std::filesystem::remove_all(progdir + "update_backup");
							}
							catch(...) {} // try to remove created files, but if it fails, only show the main error, not the one from the removal

							OpenFailedUpdateMenu(buttonBar, "Failed to extract zip, error: '" + err + "' while writing file '"+newpath+"'", false); // TODO: localize
							return;
						}
						else
						{
							FileSys::FileData data = zip->Read(i);

							if(fwrite(data.data(), 1, data.size(), f) != data.size())
							{
								std::string err = strerror(errno);
								fclose(f);
								try
								{
									std::filesystem::remove_all(progdir + "update");
									std::filesystem::remove_all(progdir + "update_backup");
								}
								catch(...) {} // try to remove created files, but if it fails, only show the main error, not the one from the removal

								OpenFailedUpdateMenu(buttonBar, "Failed to extract zip, error: '" + err + "' while writing file '"+newpath+"'", false); // TODO: localize
								return;
							}
							fclose(f);
						}

					}

				}
				catch(std::exception &e)
				{
					try
					{
						std::filesystem::remove_all(progdir + "update");
						std::filesystem::remove_all(progdir + "update_backup");
					}
					catch(...) {} // try to remove created files, but if it fails, only show the main error, not the one from the removal

					OpenFailedUpdateMenu(buttonBar, e.what(), false);
					return;
				}

				OpenPopup(buttonBar, "Updated", {"Update was successful, the launcher will now restart."}, // TODO: localize
				{
					{"Confirm", 0, [progdir](auto &self){
						CloseWidgetResources();

						// this code leaks memory but it terminates so it's fiiiiiiiiiiiiine
						int argc;
						LPWSTR * argv = CommandLineToArgvW(GetCommandLineW(), &argc);

						std::string updater_filename = (progdir + "updater.exe");

						int numchars = MultiByteToWideChar(CP_UTF8, 0, updater_filename.c_str(), updater_filename.length(), NULL, 0);

						WCHAR * updater_filename_w = new WCHAR[numchars + 1];
						MultiByteToWideChar(CP_UTF8, 0, updater_filename.c_str(), updater_filename.length(), updater_filename_w, numchars);
						updater_filename_w[numchars] = 0;

						argv[0] = updater_filename_w;
						_wexecv(updater_filename_w, argv);
					}}
				}, 500.0, false);
				#else
					#error "Updater not implemented for non-windows platforms"
				#endif

			}
		}
	}


	virtual void OnClose() override
	{
		if(downloader_thread)
		{
			CancelDownload();
			downloader_thread->join();
			downloader_thread = nullptr;

			OpenPopup(buttonBar, "Cancelled", {"Download was cancelled"}, // TODO: localize
			{
				{"Back", 0, [](auto &self){
					self.Close();
				}}
			});
		}
		else
		{
			ProgressPopup::OnClose();
		}
	}
};

update_info_t UpdateButtonBar::GetUpdateInfo(bool &ok)
{
	if(InitCurl(this))
	{
		auto doc = (UpdateChecker {}).Perform(this, CURRENT_UPDATE_CHANNEL);

		if(!doc.has_value())
		{
			goto fail;
		}

		if(!doc->IsObject())
		{
			goto jsonfail;
		}

		VersionInfo ver;

		std::string downloadUrl;

		switch(CURRENT_UPDATE_CHANNEL)
		{
		case UpdateChannel::STABLE:
			if(!doc->HasMember("tag_name") || !(*doc)["tag_name"].IsString())
			{
				goto jsonfail;
			}

			ver = VersionInfo((*doc)["tag_name"].GetString());
			break;
		case UpdateChannel::PREVIEW:
		case UpdateChannel::TESTING:
			{
				if(!doc->HasMember("assets") || !(*doc)["assets"].IsArray())
				{
					goto jsonfail;
				}

				bool release_json_found = false;
				bool download_link_found = false;
				auto arr = ((*doc)["assets"]).GetArray();

				rapidjson::Document relinfo;

				for(int i = 0; i < (int)arr.Size(); i++)
				{
					if(!arr[i].HasMember("name") || !arr[i]["name"].IsString())
					{
						goto jsonfail;
					}
					else if(std::string s = arr[i]["name"].GetString(); s == "_release.json")
					{
						if(!arr[i].HasMember("browser_download_url") || !arr[i]["browser_download_url"].IsString())
						{
							goto jsonfail;
						}

						auto release_info = (JsonDownloader {}).Perform(this, arr[i]["browser_download_url"].GetString());

						if(!release_info.has_value())
						{
							goto fail;
						}

						relinfo = std::move(*release_info);

						release_json_found = true;
					}
					#ifdef _WIN32
						else if(s.substr(0, 14) == "Windows-UZDoom" && s.substr(s.length() - 4) == ".zip")
					#else
						#error "Updater not implemented for non-windows platforms"
					#endif
					{
						if(!arr[i].HasMember("browser_download_url") || !arr[i]["browser_download_url"].IsString())
						{
							goto jsonfail;
						}

						downloadUrl = arr[i]["browser_download_url"].GetString();

						download_link_found = true;
					}
					else
					{
						continue;
					}

					if(release_json_found && download_link_found)
						break;
				}

				if(!release_json_found || !download_link_found)
				{
					goto jsonfail;
				}
				else if(!relinfo.HasMember("commit"))
				{
					goto jsonfail;
				}
				else if(!relinfo["commit"].HasMember("parent")|| !relinfo["commit"]["parent"].IsString())
				{
					goto jsonfail;
				}
				else if(!relinfo["commit"].HasMember("distance") || !relinfo["commit"]["distance"].IsString())
				{
					goto jsonfail;
				}

				ver = VersionInfo(relinfo["commit"]["parent"].GetString());
				ver.distance = atoi(relinfo["commit"]["distance"].GetString());
			}
			break;
		case UpdateChannel::RELEASE_CANDIDATE:
			//TODO RC update channel, needs to scan the releases array manually
			I_Error("RC updates not implemented");
			break;
		}

		if(!doc->HasMember("body") || !(*doc)["body"].IsString())
		{
			goto jsonfail;
		}

		ok = true;
		return update_info_t{ver, false, SplitNewLines((*doc)["body"].GetString(), (*doc)["body"].GetStringLength()), downloadUrl};
	}
	else
	{
		goto fail;
	}
jsonfail:
	OpenFailedUpdateMenu(this, "Invalid Update JSON", true);
	// fallthrough
fail:
	ok = false;
	return update_info_t{{USHRT_MAX, USHRT_MAX, USHRT_MAX}, false, {}};
}

bool isVersionInvalid(VersionInfo ver)
{
	return ver.major == USHRT_MAX || ver.minor == USHRT_MAX || ver.revision == USHRT_MAX || ver == GetCurrentVersion();
}

static void StartUpdate(UpdateButtonBar * buttonBar)
{ // TODO move to method of UpdateButtonBar
	OpenPopup<ProgressDownloader>(buttonBar, "Updating...").Perform(buttonBar, buttonBar->GetDownloadURL()); // TODO: localize
}

void UpdateButtonBar::CheckForUpdate()
{
	if(CURRENT_UPDATE_CHANNEL == UpdateChannel::RELEASE_CANDIDATE) return; //REMOVE THIS WHEN RC UPDATES ARE IMPLEMENTED

	if(!updater_check_updates_initialized)
	{
		OpenUpdateInitChoice(this);
	}
	else
	{
		if(updater_cached_update->Length() > 0)
		{
			VersionInfo cachedVer(updater_cached_update);

			if(isVersionInvalid(cachedVer))
			{
				updater_cached_update = "";
				M_SaveDefaults(NULL); // save settings
			}
			else
			{
				currentUpdate = update_info_t{cachedVer, true, {}};
			}
		}

		VersionInfo skippedVer;

		if(updater_skipped_update->Length() > 0)
		{
			VersionInfo skippedVerTmp = VersionInfo((const char *)updater_skipped_update);
			if(isVersionInvalid(skippedVerTmp))
			{
				updater_skipped_update = "";
				M_SaveDefaults(NULL); // save settings
			}
			else
			{
				skippedVer = skippedVerTmp;
			}
		}

		auto curTime = date_t::getCurrentDate();
		auto nextCheckTime = date_t::parseDate((FString)updater_last_update_check, curTime - (updater_update_interval + 1)) + updater_update_interval;

		if(curTime >= nextCheckTime || currentUpdate.has_value())
		{
			if(!currentUpdate.has_value() || curTime >= nextCheckTime) // invalidate cache if check time is due
			{
				bool ok;

				currentUpdate = GetUpdateInfo(ok);

				if(!ok) return;

				updater_last_update_check = FString(date_t::getCurrentDate());
				if(currentUpdate.has_value())
				{
					updater_cached_update = FString(currentUpdate->version);
				}
				M_SaveDefaults(NULL); // save settings
			}

			if(currentUpdate.has_value())
			{
				bool should_update;
				if(updater_debug_always_update)
				{
					should_update = true;
				}
				else if constexpr(CURRENT_UPDATE_CHANNEL == UpdateChannel::TESTING)
				{
					should_update = (currentUpdate->version != GetCurrentVersionForUpdate(CURRENT_UPDATE_CHANNEL));
				}
				else
				{
					should_update = (currentUpdate->version > GetCurrentVersionForUpdate(CURRENT_UPDATE_CHANNEL));
				}

				if(should_update && (skippedVer != currentUpdate->version))
				{
					if(updater_auto_updates)
					{
						OpenUpdateMenu(true);
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
