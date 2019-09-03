
#ifndef _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_DOTNETCORE_INTEROP_H_
#define _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_DOTNETCORE_INTEROP_H_

#include "coreclrhost.h"

#include <string>

namespace interop_dotnet_core
{
    class DotNetCoreInterop
    {
    public:
        DotNetCoreInterop();
        ~DotNetCoreInterop();
        bool Init(const char* dotnet_libs_dir);
        bool End();
        bool GetFunction(const char* assembly_name, const char* namespace_name, const char* class_name, const char* function_name, void** function_pointer);
        bool ReleaseReturn(char* return_to_release);

    private:
        bool BuildTpaList(const char* directory, const char* extension, std::string* tap_list);

    private:
        coreclr_initialize_ptr _coreclr_initialize_ptr;
        coreclr_create_delegate_ptr _coreclr_create_delegate_ptr;
        coreclr_shutdown_ptr _coreclr_shutdown_ptr;
        void* _host_handle;
        unsigned int _domain_id;
    };

}  // namespace interop_dotnet_core

#endif  // _RUN_DOTNET_CORE_V22_SRC_UNMANAGEDEXECUTABLE_DOTNETCORE_INTEROP_H_

