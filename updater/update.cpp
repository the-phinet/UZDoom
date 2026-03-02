#include "update.h"
#include <filesystem>

int main(const int argc, char** argv)
{
	if (argc < 4)
	{
		printf("Usage: updater <download_url> <exe_path> <uzdoom_pid>\n");
		return 1;
	}

	const std::string url = argv[1];
	const std::filesystem::path exe_path = argv[2];
	const int pid = std::stoi(argv[3]);

	std::error_code err;
	const std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "uzdoom_update";
	const std::filesystem::path archive_path = temp_dir / updateFilename;
	std::filesystem::remove_all(temp_dir);
	std::filesystem::create_directories(temp_dir, err);
	if (err)
	{
		printf("Failed to create temp dir: %s\n", err.message().c_str());
		return 1;
	}

	if (!DownloadBuild(url, archive_path))
	{
		printf("Failed to download update.\n");
		return 1;
	}

	AwaitProcessExit(pid);

#if defined(_WIN32) || defined(__APPLE__)
	if (!ExtractArchive(archive_path, temp_dir))
	{
		printf("Failed to extract update.\n");
		return 1;
	}
#endif

	if (!UpdateFiles(temp_dir, exe_path.parent_path()))
	{
		printf("Failed to update files.\n");
		return 1;
	}

	if (!StartApplication(exe_path, {}))
	{
		printf("Failed to restart application.\n");
		return 1;
	}

	return 0;
}
