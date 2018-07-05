// RawIsapi.cpp : Defines the exported functions for the DLL application.
//

// System bits
#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>

// Our stuff
#include "SharedMem.h"

// IIS requirements
#include <httpext.h>
#include <tchar.h>

// .Net requirements
#include <windows.h>
#include <metahost.h>
#include <corerror.h>
#pragma comment(lib, "mscoree.lib")

// Import mscorlib.tlb (Microsoft Common Language Runtime Class Library).
#import "mscorlib.tlb" raw_interfaces_only				\
    high_property_prefixes("_get","_put","_putref")		\
    rename("ReportEvent", "InteropServices_ReportEvent")
using namespace mscorlib;

void OutputToClient(EXTENSION_CONTROL_BLOCK *pEcb, const char* controlString, ...);

void SendHTMLHeader(EXTENSION_CONTROL_BLOCK *pEcb);

HRESULT RuntimeHostV4Demo2(PCWSTR pszVersion, PCWSTR pszAssemblyPath, PCWSTR pszClassName, EXTENSION_CONTROL_BLOCK *pEcb);


// Function pointer types from the managed side:
typedef int(*INT_INT_FUNC_PTR)(int);
typedef void(*VOID_FUNC_PTR)();

BOOL WINAPI GetExtensionVersion(HSE_VERSION_INFO *pVer)
{
    pVer->dwExtensionVersion = MAKELONG(HSE_VERSION_MINOR, HSE_VERSION_MAJOR);
    char *name = "RawIsapi Host";
    int i = 0;
    do { pVer->lpszExtensionDesc[i] = name[i]; i++; } while (name[i] != 0);
    return TRUE;
}

DWORD WINAPI HttpExtensionProc(EXTENSION_CONTROL_BLOCK *pEcb)
{
    SendHTMLHeader(pEcb);
    OutputToClient(pEcb, "<html><head><title>This is my output page!</title></head><body>Hello World! Output from ISAPI Extension. Calling .Net now...<pre>");

    RuntimeHostV4Demo2(L"v4.0.30319", L"C:\\Gits\\RawIsapi\\x64\\Debug\\Communicator.dll", L"Communicator.Demo", pEcb);

    OutputToClient(pEcb, "</pre>...done</body></html>");
    return HSE_STATUS_SUCCESS;
}

void OutputToClient(EXTENSION_CONTROL_BLOCK *pEcb, const char* ctrlStr, ...)
{
    DWORD buffsize;
    buffsize = strlen(ctrlStr);
    pEcb->WriteClient(pEcb->ConnID, (void*)ctrlStr, &buffsize, NULL);
}

void SendHTMLHeader(EXTENSION_CONTROL_BLOCK *pEcb)
{
    char MyHeader[4096];
    strcpy(MyHeader, "Content-type: text/html\n\n");
    pEcb->ServerSupportFunction(pEcb->ConnID, HSE_REQ_SEND_RESPONSE_HEADER, NULL, 0, (DWORD *)MyHeader);
}

//
//   FUNCTION: RuntimeHostV4Demo2(PCWSTR, PCWSTR)
//
//   PURPOSE: The function demonstrates using .NET Framework 4.0 Hosting 
//   Interfaces to host a .NET runtime, and use the ICLRRuntimeHost interface
//   that was provided in .NET v2.0 to load a .NET assembly and invoke its 
//   type. Because ICLRRuntimeHost is not compatible with .NET runtime v1.x, 
//   the requested runtime must not be v1.x.
//   
//   If the .NET runtime specified by the pszVersion parameter cannot be 
//   loaded into the current process, the function prints ".NET runtime <the 
//   runtime version> cannot be loaded", and return.
//   
//   If the .NET runtime is successfully loaded, the function loads the 
//   assembly identified by the pszAssemblyName parameter. Next, the function 
//   invokes the public static function 'int GetStringLength(string str)' of 
//   the class and print the result.
//
//   PARAMETERS:
//   * pszVersion - The desired DOTNETFX version, in the format ¡°vX.X.XXXXX¡±. 
//     The parameter must not be NULL. It¡¯s important to note that this 
//     parameter should match exactly the directory names for each version of
//     the framework, under C:\Windows\Microsoft.NET\Framework[64]. Because 
//     the ICLRRuntimeHost interface does not support the .NET v1.x runtimes, 
//     the current possible values of the parameter are "v2.0.50727" and 
//     "v4.0.30319". Also, note that the ¡°v¡± prefix is mandatory.
//   * pszAssemblyPath - The path to the Assembly to be loaded.
//   * pszClassName - The name of the Type that defines the method to invoke.
//
//   RETURN VALUE: HRESULT of the demo.
//
HRESULT RuntimeHostV4Demo2(PCWSTR pszVersion, PCWSTR pszAssemblyPath, PCWSTR pszClassName, EXTENSION_CONTROL_BLOCK *pEcb)
{
    HRESULT hr;

    ICLRMetaHost *pMetaHost = NULL;
    ICLRRuntimeInfo *pRuntimeInfo = NULL;

    // ICorRuntimeHost and ICLRRuntimeHost are the two CLR hosting interfaces
    // supported by CLR 4.0. Here we demo the ICLRRuntimeHost interface that 
    // was provided in .NET v2.0 to support CLR 2.0 new features. 
    // ICLRRuntimeHost does not support loading the .NET v1.x runtimes.
    ICLRRuntimeHost *pClrRuntimeHost = NULL;


    // 
    // Load and start the .NET runtime.
    // 

    OutputToClient(pEcb, "\r\nLoad and start the .NET runtime");

    hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&pMetaHost));
    if (FAILED(hr))
    {
        OutputToClient(pEcb, "\r\nCLRCreateInstance failed");
        goto Cleanup;
    }

    // Get the ICLRRuntimeInfo corresponding to a particular CLR version. It 
    // supersedes CorBindToRuntimeEx with STARTUP_LOADER_SAFEMODE.
    hr = pMetaHost->GetRuntime(pszVersion, IID_PPV_ARGS(&pRuntimeInfo));
    if (FAILED(hr))
    {
        OutputToClient(pEcb, "\r\nICLRMetaHost::GetRuntime failed");
        goto Cleanup;
    }

    // Check if the specified runtime can be loaded into the process. This 
    // method will take into account other runtimes that may already be 
    // loaded into the process and set pbLoadable to TRUE if this runtime can 
    // be loaded in an in-process side-by-side fashion. 
    BOOL fLoadable;
    hr = pRuntimeInfo->IsLoadable(&fLoadable);
    if (FAILED(hr))
    {
        OutputToClient(pEcb, "\r\nICLRRuntimeInfo::IsLoadable failed");
        goto Cleanup;
    }

    if (!fLoadable)
    {
        OutputToClient(pEcb, "\r\n.NET runtime cannot be loaded");
        goto Cleanup;
    }

    // Load the CLR into the current process and return a runtime interface 
    // pointer. ICorRuntimeHost and ICLRRuntimeHost are the two CLR hosting  
    // interfaces supported by CLR 4.0. Here we demo the ICLRRuntimeHost 
    // interface that was provided in .NET v2.0 to support CLR 2.0 new 
    // features. ICLRRuntimeHost does not support loading the .NET v1.x 
    // runtimes.
    hr = pRuntimeInfo->GetInterface(CLSID_CLRRuntimeHost,
        IID_PPV_ARGS(&pClrRuntimeHost));
    if (FAILED(hr))
    {
        OutputToClient(pEcb, "\r\nICLRRuntimeInfo::GetInterface failed");
        goto Cleanup;
    }

    // Start the CLR.
    hr = pClrRuntimeHost->Start();
    if (FAILED(hr))
    {
        OutputToClient(pEcb, "\r\nCLR failed to start");
        goto Cleanup;
    }

    // 
    // Load the NET assembly and call the static method GetStringLength of 
    // the type CSSimpleObject in the assembly.
    // 

    OutputToClient(pEcb, "\r\nLoading the assembly: ");

    // Munge strings. ToDo: make a version of OutputToClient that does this.
    int len = WideCharToMultiByte(CP_ACP, 0, pszAssemblyPath, wcslen(pszAssemblyPath), NULL, 0, NULL, NULL);
    char* pszAssemblyPathBuffer = new char[len + 1];
    WideCharToMultiByte(CP_ACP, 0, pszAssemblyPath, wcslen(pszAssemblyPath), pszAssemblyPathBuffer, len, NULL, NULL);
    pszAssemblyPathBuffer[len] = '\0';
    
    OutputToClient(pEcb, pszAssemblyPathBuffer);

    char buffer[1024];

    // The invoked method of ExecuteInDefaultAppDomain must have the 
    // following signature: static int pwzMethodName (String pwzArgument)
    // where pwzMethodName represents the name of the invoked method, and 
    // pwzArgument represents the string value passed as a parameter to that 
    // method. If the HRESULT return value of ExecuteInDefaultAppDomain is 
    // set to S_OK, pReturnValue is set to the integer value returned by the 
    // invoked method. Otherwise, pReturnValue is not set.

    // The static method in the .NET class to invoke.
    PCWSTR pszStaticMethodName = L"FindFunctionPointer";
    DWORD dwResult;
    PCWSTR pszStringArg = L"Shutdown";


    hr = pClrRuntimeHost->ExecuteInDefaultAppDomain(pszAssemblyPath,
        pszClassName, pszStaticMethodName, pszStringArg, &dwResult);
    if (FAILED(hr))
    {
        sprintf(buffer, "%X", hr);
        OutputToClient(pEcb, "\r\nFailed to call method.");
        switch (hr) {
        case S_OK:
            OutputToClient(pEcb, "\r\nS_OK");
            break;
        case HOST_E_CLRNOTAVAILABLE:
            OutputToClient(pEcb, "\r\nHOST_E_CLRNOTAVAILABLE");
            break;
        case HOST_E_TIMEOUT:
            OutputToClient(pEcb, "\r\nHOST_E_TIMEOUT");
            break;
        case HOST_E_NOT_OWNER:
            OutputToClient(pEcb, "\r\nHOST_E_NOT_OWNER");
            break;
        case HOST_E_ABANDONED:
            OutputToClient(pEcb, "\r\nHOST_E_ABANDONED");
            break;
        case E_FAIL:
            OutputToClient(pEcb, "\r\nE_FAIL - Non-standard error");
            break;
        case COR_E_FILENOTFOUND:
            OutputToClient(pEcb, "\r\nCOR_E_FILENOTFOUND - Check path and permissions");
            break;
        case COR_E_FILELOAD:
            OutputToClient(pEcb, "\r\nCOR_E_FILELOAD - Check path and permissions");
            break;
        case COR_E_ENDOFSTREAM:
            OutputToClient(pEcb, "\r\nCOR_E_ENDOFSTREAM - Check path and permissions");
            break;
        case COR_E_DIRECTORYNOTFOUND:
            OutputToClient(pEcb, "\r\nCOR_E_DIRECTORYNOTFOUND - Check path and permissions");
            break;
        case COR_E_PATHTOOLONG:
            OutputToClient(pEcb, "\r\nCOR_E_PATHTOOLONG - Check path and permissions");
            break;
        case COR_E_IO:
            OutputToClient(pEcb, "\r\nCOR_E_IO - Check path and permissions");
            break;
        case COR_E_OVERFLOW:
            OutputToClient(pEcb, "\r\nCOR_E_OVERFLOW - Some kind of casting or conversion error");
            // https://blogs.msdn.microsoft.com/yizhang/2010/12/17/interpreting-hresults-returned-from-netclr-0x8013xxxx/
            break;


        default:
            OutputToClient(pEcb, "\r\nUnexpected error: ");
            OutputToClient(pEcb, buffer);
            break;
        }
        goto Cleanup;
    }

    // dwResult == 1 for success, 0 is a failure
    OutputToClient(pEcb, "\r\n\r\nSUCCESS!");
    sprintf(buffer, "\r\nCall %s.%s(\"%s\") => %d\n", pszClassName, pszStaticMethodName, pszStringArg, dwResult);
    OutputToClient(pEcb, buffer);

    if (dwResult < 1) {
        OutputToClient(pEcb, "\r\n\r\nGot failure result from inside dotnet");
        goto Cleanup;
    }

    OutputToClient(pEcb, "\r\n\r\nNext, we try to make a callback...");


    //INT_INT_FUNC_PTR
    ULONGLONG nSharedMemValue = 0;
    BOOL bGotValue = GetSharedMem(&nSharedMemValue);
    if (bGotValue)
    {
        VOID_FUNC_PTR shutdownFunc = (VOID_FUNC_PTR)nSharedMemValue;
        
        if (shutdownFunc == NULL) {
            OutputToClient(pEcb, "\r\nReturned function pointer was null");
        }
        else
        {
            shutdownFunc();
            OutputToClient(pEcb, "\r\n\r\nDone");
        }

        //int result = CSharpFunc(43);
        /*OutputToClient(pEcb, "\r\n\r\nCalled to dotnet, got ");
        sprintf(buffer, "%d", result);
        OutputToClient(pEcb, buffer);*/
    }
    else {
        OutputToClient(pEcb, "\r\n\r\nFailed to get shared memory");
    }

    //pClrRuntimeHost->ExecuteInAppDomain(1, /*delegate*/(FExecuteInAppDomainCallback)delPtr, /*param*/(void*)1234);

Cleanup:

    if (pMetaHost)
    {
        pMetaHost->Release();
        pMetaHost = NULL;
    }
    if (pRuntimeInfo)
    {
        pRuntimeInfo->Release();
        pRuntimeInfo = NULL;
    }
    if (pClrRuntimeHost)
    {
        // Please note that after a call to Stop, the CLR cannot be 
        // reinitialized into the same process. This step is usually not 
        // necessary. You can leave the .NET runtime loaded in your process.
        //wprintf(L"Stop the .NET runtime\n");
        //pClrRuntimeHost->Stop();

        pClrRuntimeHost->Release();
        pClrRuntimeHost = NULL;
    }

    return hr;
}
