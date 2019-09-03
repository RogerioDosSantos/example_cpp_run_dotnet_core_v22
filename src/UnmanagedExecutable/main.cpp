#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./dotnetcore_interop.h"

using interop_dotnet_core::DotNetCoreInterop;

// Simple Function Declaration
typedef bool (__stdcall *BoolReturnFunctionPtr)();
// Complex Function Declaration
typedef int (*report_callback_ptr)(int progress);
typedef char* (__stdcall *DoWorkFunctionPtr)(const char* jobName, int iterations, int dataSize, double* data, report_callback_ptr callbackFunction);

int ReportProgressCallback(int progress);

int main(int argc, char* argv[])
{
	DotNetCoreInterop dotnetcore = DotNetCoreInterop(); 
	if (!dotnetcore.Init("C:\\Users\\roger.santos\\git\\roger\\examples\\cpp\\run_dotnet_core_v22\\src\\ManagedLibrary\\bin\\Debug\\netcoreapp2.2\\publish"))
	{
		printf("ERROR: Could not initialize .Net Core Interop.\n");
		return -1;
	}
	// Execute Simple Function BoolReturnFunction
	BoolReturnFunctionPtr bool_return_function_ptr;
	if (!dotnetcore.GetFunction("ManagedLibrary, Version=1.0.0.0", "ManagedLibraryNamespace", "ManagedClass", "BoolReturn", (void**)& bool_return_function_ptr))
	{
		printf("ERROR: Could not get the function. (Simple Function)\n");
		return -1;
	}
	bool bool_ret = bool_return_function_ptr();
	if (!bool_ret)
	{
		printf("ERROR: Got the wrong result from the delegate function.\n");
		return -1;
	}
	printf("SUCCESS: Simple Function executed properly.\n");
	// Execute Complex Function DoWork
	DoWorkFunctionPtr do_work_function_ptr;
	if (!dotnetcore.GetFunction("ManagedLibrary, Version=1.0.0.0", "ManagedLibraryNamespace", "ManagedClass", "DoWork", (void**)& do_work_function_ptr))
	{
		printf("ERROR: Could not get the function. (Complex Function)\n");
		return -1;
	}
	// Create sample data for the double[] argument of the managed method to be called
	double data[4];
	data[0] = 0;
	data[1] = 0.25;
	data[2] = 0.5;
	data[3] = 0.75;
	char* string_ret = do_work_function_ptr("Test job", 5, sizeof(data) / sizeof(double), data, ReportProgressCallback);
	if (!string_ret)
	{
		printf("ERROR: Got the wrong result from the complex function.\n");
		return -1;
	}
	printf("SUCCESS: Complex Function executed properly. Return = %s\n", string_ret);
	// Strings returned to native code must be freed by the native code
	if (!dotnetcore.ReleaseReturn(string_ret))
	{
		printf("ERROR: Could not Release the returned string.\n");
		return -1;
	}
	if(!dotnetcore.End())
	{
		printf("ERROR: Could not end the .Net Core Interop.\n");
		return -1;
	}
	printf("SUCCESS.\n");
	return 0;
}


// Callback function passed to managed code to facilitate calling back into native code with status
int ReportProgressCallback(int progress)
{
	// Just print the progress parameter to the console and return -progress
	printf("Received status from managed code: %d\n", progress);
	return -progress;
}