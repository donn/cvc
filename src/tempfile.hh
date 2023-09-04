/*
    tempfile.hh

    Copyright (C) 2023 Efabless Corporation

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string>
#if defined(__unix__)
#include <unistd.h>
#define accessible(x) (access(x, W_OK) != 0)
#else
#define accessible(x) (true)
#endif

namespace tempfile {
    std::string gettempdir() {
        // Python-style "gettempdir"
        // See https://docs.python.org/3.10/library/tempfile.html#tempfile.gettempdir for high-level behavioral description 
        static std::string result = "";
        if (result != "") {
            return result;
        }
        std::string final_tempdir = ".";

        const char *candidates[] = {
            #ifdef _WIN32
            "/TMP",
            "/TEMP",
            "C:/TMP",
            "C:/TEMP",
            #else
            "/usr/tmp"
            "/var/tmp",
            "/tmp",
            #endif
            std::getenv("TMP"),
            std::getenv("TEMP"),
            std::getenv("TMPDIR"),
        };
        for (const char *candidate: candidates) {
            // Check non-null
            if (candidate == nullptr) {
                continue;
            }
            // Check writable dir
            if (accessible(candidate)) {
                continue;
            }
            final_tempdir = std::string(candidate);
        }
        
        result = final_tempdir;
        return result;
    }
};