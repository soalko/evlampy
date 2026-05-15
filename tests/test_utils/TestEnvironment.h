#pragma once

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

class ScopedTestHome {
public:
    ScopedTestHome() {
        namespace fs = std::filesystem;
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() / ("evlampy_test_home_" + std::to_string(now));
        fs::create_directories(path_);

        const char* prevHome = std::getenv("HOME");
        if (prevHome != nullptr) {
            previousHome_ = prevHome;
            hadPrevious_ = true;
        }

#if defined(_WIN32)
        _putenv_s("HOME", path_.string().c_str());
#else
        setenv("HOME", path_.string().c_str(), 1);
#endif
    }

    ~ScopedTestHome() {
#if defined(_WIN32)
        if (hadPrevious_) {
            _putenv_s("HOME", previousHome_.c_str());
        } else {
            _putenv_s("HOME", "");
        }
#else
        if (hadPrevious_) {
            setenv("HOME", previousHome_.c_str(), 1);
        } else {
            unsetenv("HOME");
        }
#endif
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    std::string pathString() const {
        return path_.string();
    }

private:
    std::filesystem::path path_;
    std::string previousHome_;
    bool hadPrevious_ = false;
};

