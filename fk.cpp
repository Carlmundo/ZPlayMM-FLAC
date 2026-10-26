typedef struct IUnknown IUnknown;

#include "fk.hpp"

#include <string>
#include <vector>
#include <Windows.h>
#include <winver.h>

#pragma comment(lib, "version.lib")

std::vector<HMODULE> modules;

BOOL attached = false;

template <typename TChar, typename TStringGetterFunc>
std::basic_string<TChar> GetStringFromWindowsApi(TStringGetterFunc stringGetter, int initialSize = 0)
{
	if (initialSize <= 0)
	{
		initialSize = MAX_PATH;
	}

	std::basic_string<TChar> result(initialSize, 0);
	for (;;)
	{
		auto length = stringGetter(&result[0], result.length());
		if (length == 0)
		{
			return std::basic_string<TChar>();
		}

		if (length < result.length() - 1)
		{
			result.resize(length);
			result.shrink_to_fit();
			return result;
		}

		result.resize(result.length() * 2);
	}
}

std::string GetErrorMessage(DWORD errorCode) {
	// Buffer to hold the formatted error message
	char buffer[256];

	// Get the error message corresponding to the error code
	DWORD size = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,   // Use the system message table
		errorCode, // Error code from GetLastError
		0,         // Default language ID
		buffer,    // Output buffer
		sizeof(buffer), // Size of the output buffer
		nullptr    // No additional arguments
	);

	if (size == 0) {
		// If the message couldn't be retrieved, return a default message
		return "Unknown error";
	}

	return std::string(buffer, size); // Return the formatted message as a string
}

void fkAttachReal()
{
	auto moduleName = GetStringFromWindowsApi<TCHAR>([](TCHAR* buffer, int size)
		{
			HMODULE module = GetModuleHandle(NULL);

			return GetModuleFileName(module, buffer, size);
		});

	// Get executable directory.
	CHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	*(strrchr(buffer, '\\') + 1) = '\0';

	// Attempt to load all library files matching the fk*.dll search pattern.
	lstrcat(buffer, "fk*.dll");
	WIN32_FIND_DATA findFileData;
	HANDLE hFindFile = FindFirstFile(buffer, &findFileData);
	if (hFindFile == INVALID_HANDLE_VALUE)
		return;
	do
	{
		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		HMODULE hLibrary = LoadLibraryA(findFileData.cFileName);
		if (hLibrary)
		{
			modules.push_back(hLibrary);
		}
		else
		{
			auto err = GetLastError();
			std::string errorMessage = GetErrorMessage(err);

			sprintf_s(buffer, "Could not load module %s.\n%s", findFileData.cFileName, errorMessage.c_str());
			MessageBox(NULL, buffer, "FrontendKit", MB_ICONWARNING);
		}
	} while (FindNextFile(hFindFile, &findFileData));
	FindClose(hFindFile);

	attached = true;
}

extern "C" {
	void fkAttach()
	{
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fkAttachReal, NULL, 0, NULL);
	}


	void fkDetach()
	{
		if (attached) {
			// Release all loaded modules.
			for (HMODULE hModule : modules)
				FreeLibrary(hModule);
			modules.clear();
		}
	}

}
