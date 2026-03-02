#pragma once

#include <string>
#include <filesystem>
#include <vector>

#if defined(_WIN32) || defined(__APPLE__)
const std::string updateFilename = "update.zip";
#elif defined(__linux__)
const std::string updateFilename = "UZDoom.AppImage";
#endif

bool DownloadBuild(const std::string &url, const std::filesystem::path &output_path);
bool ExtractArchive(const std::filesystem::path &archivePath, const std::filesystem::path &extractDir);
bool UpdateFiles(const std::filesystem::path &source, const std::filesystem::path &target);

bool AwaitProcessExit(int pid);
bool StartApplication(const std::filesystem::path &exe_path, const std::vector<std::string> &args);

bool GetReleaseData();
bool UpdateAvailable();
void DoUpdate();
