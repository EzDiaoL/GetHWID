#include <iostream>
#include <intrin.h>
#include <iomanip>
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <sstream>
#include <chrono>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")


std::string get_cpu_id() {
    int cpu_info[4] = { -1 };
    __cpuidex(cpu_info, 0x1, 0);

    std::stringstream cpu_id;
    cpu_id << std::hex << std::uppercase
        << std::setw(8) << std::setfill('0') << cpu_info[0] << "-"
        << std::setw(8) << std::setfill('0') << cpu_info[1] << "-"
        << std::setw(8) << std::setfill('0') << cpu_info[2] << "-"
        << std::setw(8) << std::setfill('0') << cpu_info[3];

    return cpu_id.str();
}

std::string wmi_query(const std::string& wmi_class, const std::string& property) {
    HRESULT hres;
    hres = CoInitializeEx(0, COINIT_MULTITHREADED);

    if (FAILED(hres)) {
        std::cerr << "Failed to initialize COM library. Error code = 0x" << std::hex << hres << std::endl;
        return "";
    }

    IWbemLocator* wmi_pLoc = nullptr;
    IWbemServices* wmi_pSvc = nullptr;
    IWbemClassObject* wmi_pclsObj = nullptr;
    ULONG wmi_uReturn = 0;

    hres = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr
    );

    if (FAILED(hres)) {
        std::cerr << "Failed to initialize security. Error code = 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return "";
    }

    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&wmi_pLoc
    );

    if (FAILED(hres)) {
        std::cerr << "Failed to create IWbemLocator object. Error code = 0x" << std::hex << hres << std::endl;
        CoUninitialize();
        return "";
    }

    hres = wmi_pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),         // Object path of WMI namespace
        nullptr,                      // User name. NULL = current user
        nullptr,                      // User password. NULL = current
        0,                               // Locale. NULL indicates current
        NULL,                         // Security flags.
        0,                               // Authority (e.g. Kerberos)
        0,                               // Context object
        &wmi_pSvc                             // pointer to IWbemServices proxy
    );

    if (FAILED(hres)) {
        std::cerr << "Could not connect. Error code = 0x" << std::hex << hres << std::endl;
        wmi_pLoc->Release();
        CoUninitialize();
        return "";
    }

    hres = CoSetProxyBlanket(
        wmi_pSvc,                            // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,               // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,                // RPC_C_AUTHZ_xxx
        nullptr,                         // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,          // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,     // RPC_C_IMP_LEVEL_xxx
        nullptr,                         // client identity
        EOAC_NONE                        // proxy capabilities
    );

    if (FAILED(hres)) {
        std::cerr << "Could not set proxy blanket. Error code = 0x" << std::hex << hres << std::endl;
        wmi_pSvc->Release();
        wmi_pLoc->Release();
        CoUninitialize();
        return "";
    }


    std::string result;
    IEnumWbemClassObject* pEnumerator = nullptr;
    wmi_pclsObj = nullptr;
    wmi_uReturn = 0;

    hres = wmi_pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t((std::string("SELECT * FROM ") + wmi_class).c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );

    if (FAILED(hres)) {
        std::cerr << "Query failed. Error code = 0x" << std::hex << hres << std::endl;
        wmi_pSvc->Release();
        wmi_pLoc->Release();
        CoUninitialize();
        return "";
    }

    while (pEnumerator) {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &wmi_pclsObj, &wmi_uReturn);

        if (!wmi_uReturn) {
            break;
        }

        VARIANT vtProp;
        hr = wmi_pclsObj->Get(_bstr_t(property.c_str()), 0, &vtProp, 0, 0);
        result = (_bstr_t(vtProp.bstrVal));
        VariantClear(&vtProp);
        wmi_pclsObj->Release();
    }
    if (pEnumerator) {
        pEnumerator->Release();
    }

    wmi_pSvc->Release();
    wmi_pLoc->Release();
    CoUninitialize();

    return result.empty() ? "N/A" : result;
}


std::string get_motherboard_id() {
    return wmi_query("Win32_BaseBoard", "SerialNumber");
}

std::string get_drive_serial() {
    return wmi_query("Win32_DiskDrive", "SerialNumber");
}

std::string generate_hwid() {
    std::cout << "Gathering hardware information..." << std::endl;
    using namespace std::chrono;
    std::string cpu_id, motherboard_id, drive_serial;

    std::cout << "Getting CPU ID..." << std::endl;
    auto t1 = steady_clock::now();
    cpu_id = get_cpu_id();
    auto t2 = steady_clock::now();
    std::cout << "CPU ID: " << cpu_id << " (Time: " << duration_cast<milliseconds>(t2 - t1).count() << " ms)" << std::endl;

    std::cout << "Getting Motherboard ID..." << std::endl;
    t1 = steady_clock::now();
    motherboard_id = get_motherboard_id();
    t2 = steady_clock::now();
    std::cout << "Motherboard ID: " << motherboard_id << " (Time: " << duration_cast<milliseconds>(t2 - t1).count() << " ms)" << std::endl;

    std::cout << "Getting Drive Serial..." << std::endl;
    t1 = steady_clock::now();
    drive_serial = get_drive_serial();
    t2 = steady_clock::now();
    std::cout << "Drive Serial: " << drive_serial << " (Time: " << duration_cast<milliseconds>(t2 - t1).count() << " ms)" << std::endl;

    std::stringstream hwid;
    hwid << "CPU:" << cpu_id << "_"
        << "MB:" << motherboard_id << "_"
        << "DRIVE:" << drive_serial;

    return hwid.str();
}

int main() {
    std::string hwid = generate_hwid();
    std::cout << "HWID: " << hwid << std::endl;
    system("pause");
    return 0;
}