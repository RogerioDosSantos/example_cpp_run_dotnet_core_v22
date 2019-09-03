
#include "./dotnetcore_interop.h"

#include <string>

using interop_dotnet_core::DotNetCoreInterop;

#include "coreclrhost.h"

#define MANAGED_ASSEMBLY "ManagedLibrary.dll"

#ifdef WIN32
#ifndef WINDOWS
#define WINDOWS 1
#endif
#endif  // WIN32

// Define OS-specific items like the CoreCLR library's name and path elements
#if WINDOWS
#include <Windows.h>
#define FS_SEPARATOR "\\"
#define PATH_DELIMITER ";"
#define CORECLR_FILE_NAME "coreclr.dll"
#elif LINUX
#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#define FS_SEPARATOR "/"
#define PATH_DELIMITER ":"
#define MAX_PATH PATH_MAX
#if OSX
// For OSX, use Linux defines except that the CoreCLR runtime library has a different name
#define CORECLR_FILE_NAME "libcoreclr.dylib"
#else
#define CORECLR_FILE_NAME "libcoreclr.so"
#endif
#endif

int ReportProgressCallback(int progress);

DotNetCoreInterop::DotNetCoreInterop()
    : _coreclr_initialize_ptr(NULL)
    , _coreclr_create_delegate_ptr(NULL)
    , _coreclr_shutdown_ptr(NULL)
    , _host_handle(NULL)
    , _domain_id(0)
{
}

DotNetCoreInterop::~DotNetCoreInterop()
{
}

bool DotNetCoreInterop::Init(const char* dotnet_libs_dir)
{
    // Load .Net Core Runtime - We assume the the .Net Core was fully published with the libraries (Self Contained Publish)
    std::string dotnet_runtime_lib_name = "coreclr.dll";
    std::string core_clr_path(dotnet_libs_dir);
    core_clr_path.append(FS_SEPARATOR);
    core_clr_path.append(dotnet_runtime_lib_name);
#if WINDOWS
    HMODULE core_clr = LoadLibraryExA(core_clr_path.c_str(), NULL, 0);
#elif LINUX
    void* core_clr = dlopen(core_clr_path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (core_clr == NULL)
    {
        printf("ERROR: Failed to load core_clr from %s\n", core_clr_path.c_str());
        return false;
    }
    printf("Loaded .NET Core Runtime from %s\n", core_clr_path.c_str());

    // Get core_clr hosting functions
#if WINDOWS
    _coreclr_initialize_ptr = (coreclr_initialize_ptr)GetProcAddress(core_clr, "coreclr_initialize");
    _coreclr_create_delegate_ptr = (coreclr_create_delegate_ptr)GetProcAddress(core_clr, "coreclr_create_delegate");
    _coreclr_shutdown_ptr = (coreclr_shutdown_ptr)GetProcAddress(core_clr, "coreclr_shutdown");
#elif LINUX
    _coreclr_initialize_ptr = (coreclr_initialize_ptr)dlsym(core_clr, "coreclr_initialize");
    _coreclr_create_delegate_ptr = (coreclr_create_delegate_ptr)dlsym(core_clr, "coreclr_create_delegate");
    _coreclr_shutdown_ptr = (coreclr_shutdown_ptr)dlsym(core_clr, "coreclr_shutdown");
#endif
    if (_coreclr_initialize_ptr == NULL)
    {
        printf("ERROR: coreclr_initialize not found\n");
        return false;
    }
    if (_coreclr_create_delegate_ptr == NULL)
    {
        printf("ERROR: coreclr_create_delegate not found\n");
        return false;
    }
    if (_coreclr_shutdown_ptr == NULL)
    {
        printf("ERROR: coreclr_shutdown not found\n");
        return false;
    }

    // Construct the trusted platform assemblies (TPA) list. This is the list of assemblies that .NET Core can load as trusted system assemblies.
    std::string tap_list;
    if (!BuildTpaList(dotnet_libs_dir, ".dll", &tap_list))
    {
        printf("ERROR: Could not build trusted platform assemblies list\n");
        return false;
    }
    // Define CoreCLR properties
    const char* property_keys[] = {
        "TRUSTED_PLATFORM_ASSEMBLIES"  // Trusted assemblies
    };
    const char* property_values[] = {tap_list.c_str()};

    // Start the CoreCLR runtime
    // This function both starts the .NET Core runtime and creates the default (and only) AppDomain
    int hr = _coreclr_initialize_ptr(dotnet_libs_dir,  // App base path
        "DotNetHostInterop",                      // AppDomain friendly name
        sizeof(property_keys) / sizeof(char*),     // Property count
        property_keys,                             // Property names
        property_values,                           // Property values
        &_host_handle,                            // Host handle
        &_domain_id);                             // AppDomain ID
    if (hr < 0)
    {
        printf("coreclr_initialize failed - status: 0x%08x\n", hr);
        return false;
    }
    printf("CoreCLR started\n");

    return true;
}

bool DotNetCoreInterop::ReleaseReturn(char* return_to_release)
{
    if (!return_to_release)
        return false;

        // Strings returned to native code must be freed by the native code
#if WINDOWS
    CoTaskMemFree(return_to_release);
#elif LINUX
    free(return_to_release);
#endif

    return true;
}

bool DotNetCoreInterop::GetFunction(
    const char* assembly_name, const char* namespace_name, const char* class_name, const char* function_name, void** function_pointer)
{
    // The assembly name passed in the third parameter is a managed assembly name as described at
    // https://docs.microsoft.com/dotnet/framework/app-domains/assembly-names
    std::string full_class_name(namespace_name);
    full_class_name.append(".");
    full_class_name.append(class_name);
    int hr = _coreclr_create_delegate_ptr(_host_handle, _domain_id, assembly_name, full_class_name.c_str(), function_name, function_pointer);
    if (hr < 0)
    {
        printf("coreclr_create_delegate failed - status: 0x%08x\n", hr);
        return false;
    }
    printf("Managed delegate created\n");

    return true;
}

bool DotNetCoreInterop::End()
{
    // Shutdown CoreCLR
    int hr = _coreclr_shutdown_ptr(_host_handle, _domain_id);
    if (hr < 0)
    {
        printf("coreclr_shutdown failed - status: 0x%08x\n", hr);
        return false;
    }
    printf("CoreCLR successfully shutdown\n");
    return true;
}

#if WINDOWS
// Win32 directory search for .dll files
bool DotNetCoreInterop::BuildTpaList(const char* directory, const char* extension, std::string* tap_list)
{
    // This will add all files with a .dll extension to the TPA list.
    // This will include unmanaged assemblies (coreclr.dll, for example) that don't
    // belong on the TPA list. In a real host, only managed assemblies that the host
    // expects to load should be included. Having extra unmanaged assemblies doesn't
    // cause anything to fail, though, so this function just enumerates all dll's in
    // order to keep this sample concise.
    std::string search_path(directory);
    search_path.append(FS_SEPARATOR);
    search_path.append("*");
    search_path.append(extension);

    WIN32_FIND_DATAA find_data;
    HANDLE file_handle = FindFirstFileA(search_path.c_str(), &find_data);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        do
        {
            // Append the assembly to the list
            tap_list->append(directory);
            tap_list->append(FS_SEPARATOR);
            tap_list->append(find_data.cFileName);
            tap_list->append(PATH_DELIMITER);

            // Note that the CLR does not guarantee which assembly will be loaded if an assembly
            // is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
            // extensions. Therefore, a real host should probably add items to the list in priority order and only
            // add a file if it's not already present on the list.
            //
            // For this simple sample, though, and because we're only loading TPA assemblies from a single path,
            // and have no native images, we can ignore that complication.
        } while (FindNextFileA(file_handle, &find_data));
        FindClose(file_handle);
    }
    return true;
}
#elif LINUX
// POSIX directory search for .dll files
bool DotNetCoreInterop::BuildTpaList(const char* directory, const char* extension, std::string* tap_list)
{
    DIR* dir = opendir(directory);
    struct dirent* entry;
    int extension_length = strlen(extension);
    while ((entry = readdir(dir)) != NULL)
    {
        // This simple sample doesn't check for symlinks
        std::string filename(entry->d_name);

        // Check if the file has the right extension
        int extension_position = filename.length() - extension_length;
        if (extension_position <= 0 || filename.compare(extension_position, extension_length, extension) != 0)
            continue;

        // Append the assembly to the list
        tap_list->append(directory);
        tap_list->append(FS_SEPARATOR);
        tap_list->append(filename);
        tap_list->append(PATH_DELIMITER);

        // Note that the CLR does not guarantee which assembly will be loaded if an assembly
        // is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
        // extensions. Therefore, a real host should probably add items to the list in priority order and only
        // add a file if it's not already present on the list.
        //
        // For this simple sample, though, and because we're only loading TPA assemblies from a single path,
        // and have no native images, we can ignore that complication.
        return true;
    }
}
#endif
