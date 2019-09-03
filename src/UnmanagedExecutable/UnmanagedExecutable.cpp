
#include <iostream>
#include <string>

#include "./typedefs.hpp"

using std::string;

string GetCurrentDir()
{
	// Get Current Executable Directory
	char runtimePath[MAX_PATH];
#if WINDOWS
	GetFullPathNameA(argv[0], MAX_PATH, runtimePath, NULL);
#elif LINUX
	realpath(argv[0], runtimePath);
#endif
	char* last_slash = strrchr(runtimePath, FS_SEPARATOR[0]);
	if (last_slash != NULL)
		* last_slash = 0;
	return string(runtimePath);
}

CoreCLRModule LoadCoreCLR()
{
	// Contruct CLR path (coreclr.dll/libcoreclr.so)
	std::string coreClrPath(GetCurrentDir());
	coreClrPath.append(FS_SEPARATOR);
	coreClrPath.append(CORECLR_FILE_NAME);
#if WINDOWS
	CoreCLRModule coreClrModule = LoadLibraryExA(coreClrPath.c_str(), NULL, 0);
#elif LINUX
	CoreCLRModule coreClrModule = dlopen(coreClrPath.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
	if (coreClrModule == NULL)
	{
		printf("ERROR: Failed to load CoreCLR from %s\n", coreClrPath.c_str());
		return NULL;
	}
	printf("Loaded CoreCLR from %s\n", coreClrPath.c_str());
	return coreClrModule;
}

int main()
{	
	// Construct the managed library path
	std::string managedLibraryPath(GetCurrentDir());
	managedLibraryPath.append(FS_SEPARATOR);
	managedLibraryPath.append("netstandard2.0");
	managedLibraryPath.append(FS_SEPARATOR);
	managedLibraryPath.append("ManagedLibrary.dll");
	// STEP 1: Load CoreCLR (coreclr.dll/libcoreclr.so)
	CoreCLRModule coreClrModule = LoadCoreCLR();
	if (coreClrModule == NULL)
		return -1;

	// STEP 2: Get CoreCLR hosting functions
#if WINDOWS
// <Snippet2>
	coreclr_initialize_ptr initializeCoreClr = (coreclr_initialize_ptr)GetProcAddress(coreClr, "coreclr_initialize");
	coreclr_create_delegate_ptr createManagedDelegate = (coreclr_create_delegate_ptr)GetProcAddress(coreClr, "coreclr_create_delegate");
	coreclr_shutdown_ptr shutdownCoreClr = (coreclr_shutdown_ptr)GetProcAddress(coreClr, "coreclr_shutdown");
	// </Snippet2>
#elif LINUX
	coreclr_initialize_ptr initializeCoreClr = (coreclr_initialize_ptr)dlsym(coreClr, "coreclr_initialize");
	coreclr_create_delegate_ptr createManagedDelegate = (coreclr_create_delegate_ptr)dlsym(coreClr, "coreclr_create_delegate");
	coreclr_shutdown_ptr shutdownCoreClr = (coreclr_shutdown_ptr)dlsym(coreClr, "coreclr_shutdown");
#endif

	if (initializeCoreClr == NULL)
	{
		printf("coreclr_initialize not found");
		return -1;
	}

	if (createManagedDelegate == NULL)
	{
		printf("coreclr_create_delegate not found");
		return -1;
	}

	if (shutdownCoreClr == NULL)
	{
		printf("coreclr_shutdown not found");
		return -1;
	}

	// STEP 3: Construct properties used when starting the runtime

	// Construct the trusted platform assemblies (TPA) list
	// This is the list of assemblies that .NET Core can load as
	// trusted system assemblies.
	// For this host (as with most), assemblies next to CoreCLR will
	// be included in the TPA list
	std::string tpaList;
	BuildTpaList(GetCurrentDir(), ".dll", tpaList);

	// Define CoreCLR properties
	// Other properties related to assembly loading are common here,
	// but for this simple sample, TRUSTED_PLATFORM_ASSEMBLIES is all
	// that is needed. Check hosting documentation for other common properties.
	const char* propertyKeys[] = {
		"TRUSTED_PLATFORM_ASSEMBLIES"      // Trusted assemblies
	};
	const char* propertyValues[] = {
		tpaList.c_str()
	};

	// STEP 4: Start the CoreCLR runtime
	void* hostHandle;
	unsigned int domainId;

	// This function both starts the .NET Core runtime and creates
	// the default (and only) AppDomain
	int hr = initializeCoreClr(
		runtimePath,        // App base path
		"SampleHost",       // AppDomain friendly name
		sizeof(propertyKeys) / sizeof(char*),   // Property count
		propertyKeys,       // Property names
		propertyValues,     // Property values
		&hostHandle,        // Host handle
		&domainId);         // AppDomain ID
	if (hr >= 0)
	{
		printf("CoreCLR started\n");
	}
	else
	{
		printf("coreclr_initialize failed - status: 0x%08x\n", hr);
		return -1;
	}

	// STEP 5: Create delegate to managed code and invoke it
	doWork_ptr managedDelegate;
	// The assembly name passed in the third parameter is a managed assembly name
	// as described at https://docs.microsoft.com/dotnet/framework/app-domains/assembly-names
	hr = createManagedDelegate(
		hostHandle,
		domainId,
		"ManagedLibrary, Version=1.0.0.0",
		"ManagedLibrary.ManagedWorker",
		"DoWork",
		(void**)& managedDelegate);
	if (hr >= 0)
	{
		printf("Managed delegate created\n");
	}
	else
	{
		printf("coreclr_create_delegate failed - status: 0x%08x\n", hr);
		return -1;
	}
	// Create sample data for the double[] argument of the managed method to be called
	double data[4];
	data[0] = 0;
	data[1] = 0.25;
	data[2] = 0.5;
	data[3] = 0.75;
	// Invoke the managed delegate and write the returned string to the console
	char* ret = managedDelegate("Test job", 5, sizeof(data) / sizeof(double), data, ReportProgressCallback);

	printf("Managed code returned: %s\n", ret);

	// Strings returned to native code must be freed by the native code
#if WINDOWS
	CoTaskMemFree(ret);
#elif LINUX
	free(ret);
#endif

	//
	// STEP 6: Shutdown CoreCLR
	//

	// <Snippet6>
	hr = shutdownCoreClr(hostHandle, domainId);
	// </Snippet6>

	if (hr >= 0)
	{
		printf("CoreCLR successfully shutdown\n");
	}
	else
	{
		printf("coreclr_shutdown failed - status: 0x%08x\n", hr);
	}

	return 0;
}

#if WINDOWS
// Win32 directory search for .dll files
// <Snippet7>
void BuildTpaList(const char* directory, const char* extension, std::string& tpaList)
{
	// This will add all files with a .dll extension to the TPA list.
	// This will include unmanaged assemblies (coreclr.dll, for example) that don't
	// belong on the TPA list. In a real host, only managed assemblies that the host
	// expects to load should be included. Having extra unmanaged assemblies doesn't
	// cause anything to fail, though, so this function just enumerates all dll's in
	// order to keep this sample concise.
	std::string searchPath(directory);
	searchPath.append(FS_SEPARATOR);
	searchPath.append("*");
	searchPath.append(extension);

	WIN32_FIND_DATAA findData;
	HANDLE fileHandle = FindFirstFileA(searchPath.c_str(), &findData);

	if (fileHandle != INVALID_HANDLE_VALUE)
	{
		do
		{
			// Append the assembly to the list
			tpaList.append(directory);
			tpaList.append(FS_SEPARATOR);
			tpaList.append(findData.cFileName);
			tpaList.append(PATH_DELIMITER);

			// Note that the CLR does not guarantee which assembly will be loaded if an assembly
			// is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
			// extensions. Therefore, a real host should probably add items to the list in priority order and only
			// add a file if it's not already present on the list.
			//
			// For this simple sample, though, and because we're only loading TPA assemblies from a single path,
			// and have no native images, we can ignore that complication.
		} while (FindNextFileA(fileHandle, &findData));
		FindClose(fileHandle);
	}
}
// </Snippet7>
#elif LINUX
// POSIX directory search for .dll files
void BuildTpaList(const char* directory, const char* extension, std::string& tpaList)
{
	DIR* dir = opendir(directory);
	struct dirent* entry;
	int extLength = strlen(extension);

	while ((entry = readdir(dir)) != NULL)
	{
		// This simple sample doesn't check for symlinks
		std::string filename(entry->d_name);

		// Check if the file has the right extension
		int extPos = filename.length() - extLength;
		if (extPos <= 0 || filename.compare(extPos, extLength, extension) != 0)
		{
			continue;
		}

		// Append the assembly to the list
		tpaList.append(directory);
		tpaList.append(FS_SEPARATOR);
		tpaList.append(filename);
		tpaList.append(PATH_DELIMITER);

		// Note that the CLR does not guarantee which assembly will be loaded if an assembly
		// is in the TPA list multiple times (perhaps from different paths or perhaps with different NI/NI.dll
		// extensions. Therefore, a real host should probably add items to the list in priority order and only
		// add a file if it's not already present on the list.
		//
		// For this simple sample, though, and because we're only loading TPA assemblies from a single path,
		// and have no native images, we can ignore that complication.
	}
}
#endif

// Callback function passed to managed code to facilitate calling back into native code with status
int ReportProgressCallback(int progress)
{
	// Just print the progress parameter to the console and return -progress
	printf("Received status from managed code: %d\n", progress);
	return -progress;
}