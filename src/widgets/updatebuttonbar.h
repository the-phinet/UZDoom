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
#include <optional>
#include "basics.h"

class LauncherWindow;
class PushButton;

struct date_t
{
	static date_t getCurrentDate();
	static date_t parseDate(FString str, date_t fallback);

	static constexpr bool isLeapYear(int year)
	{
		return ((year % 4) == 0) && (((year % 100) != 0) || ((year % 400) == 0));
	}

	static constexpr int dayCountYear(int year)
	{
		return isLeapYear(year) ? 366 : 365;
	}

	static constexpr int dayCount(int year, int month)
	{
		if(month == 2)
		{
			return isLeapYear(year) ? 29 : 28;
		}
		else
		{
			return (month > 7) ? 30 + (month % 2) : 31 - (month % 2);
		}
	}

	constexpr int dayCount() const
	{
		return dayCount(year, month);
	}

	int day; // 1-31
	int month; // 1-12
	int year;

	constexpr date_t& operator+=(int days)
	{
		day--; // 0-based
		day += days;
		while(day >= dayCount())
		{
			day -= dayCount();
			month++;
			if(month > 12)
			{
				month = 1;
				year++;
			}
		}
		day++; // 1-based

		return *this;
	}

	constexpr date_t operator+(int days) const
	{
		date_t tmp = *this;
		tmp += days;
		return tmp;
	}

	constexpr date_t& operator-=(int days)
	{
		day--; // 0-based
		day -= days;

		while(day < 0)
		{
			month--;
			if(month < 1)
			{
				month = 12;
				year--;
			}
			day += dayCount();
		}
		day++; // 1-based

		return *this;
	}

	constexpr date_t operator-(int days) const
	{
		date_t tmp = *this;
		tmp -= days;
		return tmp;
	}

	//difference in days
	constexpr int operator-(date_t other) const
	{
		date_t tmp = *this;
		tmp.day--; // 0-based
		other.day--; // 0-based
		while(tmp.year != other.year)
		{
			if(tmp.year > other.year)
			{
				other.day += dayCountYear(other.year);
				other.year++;
			}
			else
			{
				tmp.day += dayCountYear(tmp.year);
				tmp.year++;
			}
		}

		while(tmp.month != other.month)
		{
			if(tmp.month > other.month)
			{
				other.month += dayCount(other.year, other.month);
				other.month++;
			}
			else
			{
				tmp.month += dayCount(tmp.year, tmp.month);
				tmp.month++;
			}
		}

		return tmp.day - other.day;
	}

	constexpr std::strong_ordering operator<=>(const date_t &other) const
	{
		if(other.year != year)
		{
			return year <=> other.year;
		}
		else if(other.month != month)
		{
			return month <=> other.month;
		}
		else
		{
			return day <=> other.day;
		}
	}

	explicit operator FString() const
	{
		FString tmp;
		tmp.Format("%d-%d-%d", year, month, day);
		return tmp;
	}
};

struct update_info_t
{
	VersionInfo version;

	bool cached = false;

	std::vector<std::string> release_notes;

	std::string download_url;
};

class UpdateButtonBar : public Widget
{
	public:
		UpdateButtonBar(LauncherWindow *parent);
		void UpdateLanguage();

		double GetPreferredHeight() const;

		void CheckForUpdate();

		int GetMargin() { return 4; }

	protected:
		void OnPaint(Canvas* canvas) override;
		void OnMouseMove(const Point& pos) override;
		void OnMouseLeave() override;
		bool OnMouseDown(const Point& pos, InputKey key) override;
		bool OnMouseUp(const Point& pos, InputKey key) override;
		void OnUpdateButtonClicked();

		LauncherWindow *GetLauncher() const;

		update_info_t GetUpdateInfo(bool &ok);

		FString text;

		bool close_highlighted = false;
		bool pressed = false;
		bool pressed_hover = false;
		bool close_pressed = false;
		bool close_pressed_hover = false;

		std::shared_ptr<Image> arrow;
		std::shared_ptr<Image> close;

		std::optional<update_info_t> currentUpdate;

		friend void OpenDismissUpdateMenu(UpdateButtonBar * self, bool isAutoUpdate);
		friend void OpenUpdateMenu(UpdateButtonBar * self, bool isAutoUpdate);
	public:
		std::string GetDownloadURL() { return currentUpdate->download_url; }
	private:
		void OpenUpdateInitChoice();
		void OpenUpdateIntervalChoice();
		void OpenUpdateMenu(bool isAutoUpdate);
		void OpenDismissUpdateMenu(bool isAutoUpdate);
		void OpenFailedUpdateMenu(const std::string &err, bool checker);
		void StartUpdate();
		FString UpdateToString();
		bool InitCurl();

		bool curl_initialized = false;
		bool curl_initialized_ok = false;

		friend class JsonDownloader;
		friend class ProgressDownloader;
		friend class UpdateChecker;
};
