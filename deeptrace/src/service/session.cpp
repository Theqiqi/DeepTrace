#include "service/session.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

namespace deeptrace {

const char* result_message(Result r) {
    switch (r) {
        case Result::Ok: return "Ok";
        case Result::Error: return "Error";
        case Result::InvalidArg: return "InvalidArg";
        case Result::NotAttached: return "NotAttached";
        case Result::NoSuchProcess: return "NoSuchProcess";
        case Result::AccessDenied: return "AccessDenied";
        case Result::ReadFault: return "ReadFault";
        case Result::WriteFault: return "WriteFault";
        case Result::NotFound: return "NotFound";
        case Result::Timeout: return "Timeout";
        case Result::NotSupported: return "NotSupported";
        case Result::AlreadyExists: return "AlreadyExists";
        case Result::NotExecutable: return "NotExecutable";
        case Result::BadFormat: return "BadFormat";
    }
    return "Unknown";
}

}  // namespace deeptrace

namespace deeptrace::internal {

namespace {
Session g_session;
}

Session& session() { return g_session; }

std::string state_dir(uint32_t pid) {
    char temp[MAX_PATH] = {0};
    ::GetTempPathA(MAX_PATH, temp);
    std::string dir = std::string(temp) + "deeptrace_" + std::to_string(pid);
    ::CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

}  // namespace deeptrace::internal
