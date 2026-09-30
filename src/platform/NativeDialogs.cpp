#include "NativeDialogs.h"

#include <windows.h>
#include <shobjidl.h>

std::optional<std::filesystem::path>
NativeDialogs::pickFolder()
{
    HRESULT result = CoInitializeEx(
        nullptr,
        COINIT_APARTMENTTHREADED |
        COINIT_DISABLE_OLE1DDE);

    bool shouldUninitialize = SUCCEEDED(result);

    if (FAILED(result) && result != RPC_E_CHANGED_MODE)
    {
        return std::nullopt;
    }

    IFileDialog* dialog = nullptr;

    result = CoCreateInstance(
        CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));

    if (FAILED(result))
    {
        if (shouldUninitialize)
        {
            CoUninitialize();
        }

        return std::nullopt;
    }

    DWORD options = 0;

    result = dialog->GetOptions(&options);

    if (SUCCEEDED(result))
    {
        result = dialog->SetOptions(
            options |
            FOS_PICKFOLDERS |
            FOS_FORCEFILESYSTEM |
            FOS_PATHMUSTEXIST);
    }

    if (SUCCEEDED(result))
    {
        result = dialog->Show(nullptr);
    }

    if (FAILED(result))
    {
        dialog->Release();

        if (shouldUninitialize)
        {
            CoUninitialize();
        }

        // User cancelling the dialog also comes here.
        return std::nullopt;
    }

    IShellItem* item = nullptr;

    result = dialog->GetResult(&item);

    dialog->Release();

    if (FAILED(result))
    {
        if (shouldUninitialize)
        {
            CoUninitialize();
        }

        return std::nullopt;
    }

    PWSTR path = nullptr;

    result = item->GetDisplayName(
        SIGDN_FILESYSPATH,
        &path);

    item->Release();

    if (FAILED(result))
    {
        if (shouldUninitialize)
        {
            CoUninitialize();
        }

        return std::nullopt;
    }

    std::filesystem::path selectedPath(path);

    CoTaskMemFree(path);

    if (shouldUninitialize)
    {
        CoUninitialize();
    }

    return selectedPath;
}