#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <array>
#include <climits>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#include "../src/version.h"
#include "update.h"

#include <../src/common/thirdparty/rapidjson/rapidjson.h>
#include <../src/common/thirdparty/rapidjson/reader.h>
#include <../src/common/thirdparty/rapidjson/istreamwrapper.h>

#ifdef _WIN32
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "zlib.lib")
#define CURL_STATICLIB
#endif
#include <curl/curl.h>

#include <miniz.h>

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <sys/wait.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

const std::string ghURL = "https://api.github.com/repos/UZDoom/UZDoom/releases/latest";

struct ReleaseReaderHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, ReleaseReaderHandler>
{
	bool foundAssetList = false;
	bool foundAsset = false;

	int depth = 0;

	std::string curKey;
	std::string curAssetName;
	std::string curAssetURL;

	std::string latestVersion;
	std::string downloadURL;

	bool Key(const char* str, rapidjson::SizeType len, bool copy)
	{
		curKey.assign(str, len);
		return true;
	}

	bool StartArray()
	{
		if (curKey == "assets")
		{
			foundAssetList = true;
		}
		return true;
	}

	bool EndArray(rapidjson::SizeType)
	{
		foundAssetList = false;
		return true;
	}

	bool StartObject()
	{
		if (foundAssetList)
		{
			if (depth == 0)
			{
				foundAsset = true;
				curAssetName.clear();
				curAssetURL.clear();
			}
			++depth;
		}
		return true;
	}

	bool EndObject(rapidjson::SizeType)
	{
		if (foundAssetList)
		{
			--depth;
			if (foundAsset && depth == 0)
			{
#ifdef _WIN32
				if (curAssetName.starts_with("Windows-UZDoom"))
#elif defined(__APPLE__)
				if (curAssetName.starts_with("macOS-UZDoom"))
#elif defined(__linux__)
				if (curAssetName.starts_with("Linux-UZDoom"))
#endif
				{
					downloadURL = curAssetURL;
					foundAsset = foundAssetList = false;
					return false;
				}
			}
		}
		return true;
	}

	bool String(const char* str, const rapidjson::SizeType len, bool copy)
	{
		const std::string val(str, len);

		if (curKey == "tag_name")
		{
			latestVersion = val;
		}

		if (foundAsset)
		{
			if (curKey == "name")
			{
				curAssetName = val;
			}
			else if (curKey == "browser_download_url")
			{
				curAssetURL = val;
			}
		}

		return true;
	}

	bool Null() { return true; }
	bool Bool(bool) { return true; }
	bool Int(int) { return true; }bool Uint(unsigned) { return true; }
    bool Int64(int64_t) { return true; }
    bool Uint64(uint64_t) { return true; }
    bool Double(double) { return true; }
} rHandler;

static size_t WriteToString(void* contents, const size_t size, const size_t nmemb, void* userp)
{
	const size_t totalSize = size * nmemb;
	std::string* str = static_cast<std::string*>(userp);
	str->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

static size_t WriteToFile(void* contents, const size_t size, const size_t nmemb, void* userp)
{
	const size_t totalSize = size * nmemb;
	std::ofstream* str = static_cast<std::ofstream*>(userp);
	str->write(static_cast<char*>(contents), static_cast<std::streamsize>(totalSize));
	return totalSize;
}

static std::filesystem::path GetCurrentExePath()
{
	char buf[PATH_MAX];
#ifdef _WIN32
	if (GetModuleFileNameA(NULL, buf, PATH_MAX) == 0)
	{
		return std::string();
	}
	return std::string(buf);
#elif __linux__
	/*const long len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len == -1)
	{
		return std::string();
	}

	buf[len] = '\0';
	return std::string(buf);*/

	return std::string(std::getenv("APPIMAGE"));
#elif __APPLE__
	uint32_t size = PATH_MAX;
	if (_NSGetExecutablePath(buf, &size))
	{
		return std::string(buf);
	}

	return std::string();
#endif
}

bool GetReleaseData()
{
	CURL* curl = curl_easy_init();
	std::string response;

	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, ghURL.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "UZDoom Updater");
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

		CURLcode res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			std::string errorMsg = std::string("Update failed: ") + curl_easy_strerror(res);
			fprintf(stderr, "%s\n", errorMsg.c_str());
			curl_easy_cleanup(curl);
			return false;
		}
		curl_easy_cleanup(curl);
	}

	if (response.empty())
	{
		fprintf(stderr, "Failed to fetch update information\n");
		return false;
	}

	rapidjson::Reader parser;
	rapidjson::StringStream jsonStream(response.c_str());
	parser.Parse(jsonStream, rHandler);
	if (rHandler.latestVersion.empty() || rHandler.downloadURL.empty())
	{
		fprintf(stderr, "Invalid update information format\n");
		return false;
	}

	return true;
}

bool UpdateAvailable()
{
	std::array<int, 3> curVer{ VER_MAJOR, VER_MINOR, VER_REVISION };
	std::array<int, 3> upstreamVer{};
	std::istringstream verStream(rHandler.latestVersion);
	char dot = 0;

	if (!(verStream >> upstreamVer[0] >> dot >> upstreamVer[1] >> dot >> upstreamVer[2]))
	{
		fprintf(stderr, "Invalid version format: %s\n", rHandler.latestVersion.c_str());
		return false;
	}

	if (curVer > upstreamVer)
	{
		return false;
	}

	return true;
}

bool DownloadBuild(const std::string& url, const std::filesystem::path& output_path)
{
	CURL* curl = curl_easy_init();
	if (!curl)
		return false;

	std::ofstream package(output_path, std::ios::binary);
	if (!package.is_open())
	{
		curl_easy_cleanup(curl);
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToFile);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &package);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "UZDoom Updater");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	const CURLcode res = curl_easy_perform(curl);
	package.close();
	curl_easy_cleanup(curl);
	return res == CURLE_OK;
}

bool ExtractArchive(const std::filesystem::path& archivePath, const std::filesystem::path& extractDir)
{
	mz_zip_archive archive = {};

	if (!mz_zip_reader_init_file(&archive, archivePath.string().c_str(), 0))
	{
		fprintf(stderr, "Failed to open archive: %s\n", mz_zip_get_error_string(mz_zip_get_last_error(&archive)));
		return false;
	}

	mz_uint numFiles = mz_zip_reader_get_num_files(&archive);
	if (numFiles == 0)
	{
		fprintf(stderr, "Archive is empty\n");
		mz_zip_reader_end(&archive);
		return false;
	}

	for (mz_uint i = 0; i < numFiles; ++i)
	{
		char filename[PATH_MAX];
		mz_zip_reader_get_filename(&archive, i, filename, sizeof(filename));
		if (!filename)
		{
			fprintf(stderr, "Failed to get filename for index %u: %s\n", i, mz_zip_get_error_string(mz_zip_get_last_error(&archive)));
			mz_zip_reader_end(&archive);
			return false;
		}

		std::filesystem::path targetPath = extractDir / filename;
		if (mz_zip_reader_is_file_a_directory(&archive, i))
		{
			std::filesystem::create_directories(targetPath);
			continue;
		}

		std::error_code err;
		std::filesystem::create_directories(targetPath.parent_path(), err);
		if (err)
		{
			fprintf(stderr, "Failed to create directory %s: %s\n", targetPath.parent_path().string().c_str(), err.message().c_str());
			mz_zip_reader_end(&archive);
			return false;
		}

		if (!mz_zip_reader_extract_to_file(&archive, i, targetPath.string().c_str(), 0))
		{
			fprintf(stderr, "Failed to extract file %s: %s\n", targetPath.string().c_str(), mz_zip_get_error_string(mz_zip_get_last_error(&archive)));
			mz_zip_reader_end(&archive);
			return false;
		}
	}

	mz_zip_reader_end(&archive);
	return true;
}

bool UpdateFiles(const std::filesystem::path& source, const std::filesystem::path& target)
{
#if defined(_WIN32) || defined(__APPLE__)
	for (auto& entry : std::filesystem::recursive_directory_iterator(source))
	{
		const auto& path = entry.path();
		auto rel_path = std::filesystem::relative(path, source);
		auto target_path = target / rel_path;

		if (path.filename() == updateFilename)
		{
			continue;
		}

		if (entry.is_directory())
		{
			std::filesystem::create_directories(target_path);
		}
		else if (entry.is_regular_file())
		{
			std::error_code err;

			std::filesystem::create_directories(target_path.parent_path());
			std::filesystem::copy_file(path, target_path, std::filesystem::copy_options::overwrite_existing ,err);

			if (err)
			{
				fprintf(stderr, "Failed to copy %s to %s: %s\n", path.string().c_str(), target_path.string().c_str(), err.message().c_str());
				return false;
			}
		}
	}

	std::filesystem::remove_all(source);
#elif defined(__linux__)
	std::error_code err;
	std::filesystem::path appImageFile;

	for (auto& entry : std::filesystem::directory_iterator(source))
	{
		if (entry.is_regular_file())
		{
			appImageFile = entry.path();
			break;
		}
	}

	if (appImageFile.empty())
	{
		fprintf(stderr, "No AppImage file found in update package\n");
		return false;
	}

	const std::filesystem::path targetFile = target / std::filesystem::relative(appImageFile, source);
	std::filesystem::copy_file(appImageFile, targetFile, std::filesystem::copy_options::overwrite_existing ,err);
	if (err)
	{
		fprintf(stderr, "Failed to copy %s to %s: %s\n", appImageFile.string().c_str(), targetFile.string().c_str(), err.message().c_str());
		return false;
	}

	const std::filesystem::perms newPerms = std::filesystem::status(targetFile).permissions() |
		std::filesystem::perms::owner_exec |
		std::filesystem::perms::group_exec |
		std::filesystem::perms::others_exec;
	std::filesystem::permissions(targetFile, newPerms, err);
	if (err)
	{
		fprintf(stderr, "Failed to set executable permissions on %s: %s\n", targetFile.string().c_str(), err.message().c_str());
	}

    std::filesystem::remove_all(source);
#endif

	return true;
}

bool AwaitProcessExit(int pid)
{
#ifdef _WIN32
	HANDLE hProc = OpenProcess(SYNCHRONIZE, FALSE, pid);
	if (!hProc)
	{
		return false;
	}

	WaitForSingleObject(hProc, INFINITE);
	CloseHandle(hProc);
	return true;
#elif defined(__APPLE__) || defined(__linux__)
	while (true)
	{
		if (kill(pid, 0) == -1)
		{
			if (errno == ESRCH)
			{
				return true;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
#endif
}

bool StartApplication(const std::filesystem::path &exe_path, const std::vector<std::string>& args)
{
#ifdef _WIN32
	STARTUPINFO si = {sizeof(si)};
	PROCESS_INFORMATION pi;
	std::wstring wCmdLine = L"\"" + exe_path.wstring() + L"\"";

	for (const auto& arg : args)
	{
		int wArgSize = MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, nullptr, 0);
		std::wstring wArg(wArgSize, 0);

		MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, &wArg[0], wArgSize);
		wArg.pop_back();

		wCmdLine += L" \"" + wArg + L"\"";
	}

	std::vector<wchar_t> cmdLine(wCmdLine.begin(), wCmdLine.end());
	cmdLine.push_back(L'\0');

	if (!CreateProcessW(exe_path.wstring().c_str(), cmdLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
	{
		return false;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return true;
#elif defined(__APPLE__) || defined(__linux__)
	const pid_t pid = fork();
	if (pid == 0)
	{
		std::vector<char*> argv = { const_cast<char*>(exe_path.c_str()) };
		for (auto& arg : args)
		{
			argv.push_back(const_cast<char*>(arg.c_str()));
		}
		argv.push_back(nullptr);

		execvp(exe_path.c_str(), argv.data());

		::_exit(EXIT_FAILURE);
	}
	return pid > 0;
#endif
}

void DoUpdate()
{
	std::filesystem::path exePath = GetCurrentExePath();
	if (exePath.empty())
	{
		fprintf(stderr, "Failed to get current executable path\n");
		return;
	}

#if defined(_WIN32)
	DWORD pid = GetCurrentProcessId();
	std::filesystem::path updater = exePath.parent_path() / "update.exe";
	std::vector<std::string> args = { rHandler.downloadURL, exePath.string(), std::to_string(pid) };
	StartApplication(updater, args);
#elif defined(__APPLE__) || defined(__linux__)
	pid_t pid = getpid();
	std::vector<std::string> args{ rHandler.downloadURL, exePath.string(), std::to_string(pid)};
	StartApplication(exePath.parent_path() / "update", args);
#endif
}
