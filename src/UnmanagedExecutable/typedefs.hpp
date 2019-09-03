#ifndef _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_TYPEDEFS_HPP_
#define _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_TYPEDEFS_HPP_

//typedef const char* AnsiStringParam;

// Define OS-specific items like the CoreCLR library's name and path elements
#if WINDOWS
#include <Windows.h>
#define FS_SEPARATOR "\\"
#define PATH_DELIMITER ";"
#define CORECLR_FILE_NAME "coreclr.dll"
typedef HMODULE CoreCLRModule;
#elif LINUX
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#define FS_SEPARATOR "/"
#define PATH_DELIMITER ":"
#define MAX_PATH PATH_MAX
typedef void* CoreCLRModule;
#if OSX
// For OSX, use Linux defines except that the CoreCLR runtime
// library has a different name
#define CORECLR_FILE_NAME "libcoreclr.dylib"
#else
#define CORECLR_FILE_NAME "libcoreclr.so"
#endif
#endif

#endif // _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_TYPEDEFS_HPP_

