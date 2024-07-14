#pragma once
#include <exception>
#include <string>
#include <comdef.h>
#include <iostream>

#define EFG_CHECK_RESULT(fn) \
    { \
        EfgResult result = (fn); \
        if (result != EfgResult_NoError) { \
            return result; \
        } \
    }

#define EFG_INTERNAL_TRY(fn) \
    do { \
        try { \
            (fn); \
        } \
        catch (const EfgException& ex) \
        { \
            efg->CheckD3DErrors(); \
            ex.Print(); \
            return EfgResult_InternalError; \
        } \
    } while(0)

#define EFG_INTERNAL_TRY_RET(fn) \
    do { \
        try { \
            return (fn); \
        } \
        catch (const EfgException& ex) \
        { \
            efg->CheckD3DErrors(); \
            ex.Print(); \
        } \
    } while(0)

#define EFG_D3D_TRY(hr) \
    do { \
        if (FAILED(hr)) { \
            throw EfgException(hr); \
        } \
    } while (0)

#define EFG_SHOW_ERROR(msg) \
    do { \
        std::cerr << "Error: " << msg << std::endl; \
    } while (0)

class EfgException : public std::exception
{
public:
    EfgException(HRESULT hr)
        : result(hr), message(HrToString(hr)) {}

    const char* what() const noexcept override
    {
        return message.c_str();
    }

    HRESULT GetErrorCode() const { return result; }

    void Print() const
    {
        std::cerr << "Caught Exception: " << this->what() << std::endl;
    }

private:
    // Function to convert std::wstring to std::string
    std::string WideStringToString(const std::wstring& wstr)
    {
        if (wstr.empty())
        {
            return std::string();
        }
    
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size_needed, NULL, NULL);
    
        return str;
    }
    
    // Function to convert HRESULT to std::string
    inline std::string HrToString(HRESULT hr)
    {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
#ifdef UNICODE
        std::wstring wstr(errMsg);
        return WideStringToString(wstr);
#else
        return std::string(errMsg);
#endif
    }

    HRESULT result;
    std::string message;
};
