# threadpark

A C99-compatible library for efficiently parking and un-parking threads using futex-like APIs on all major platforms.
threadpark implements backends for Windows, macOS, Linux, and FreeBSD & OpenBSD.

## Example usage

The following example showcases the threadpark API:

```cpp
#include <threadpark.h>

#include <iostream>
#include <thread>

int main() {
    tpark_handle_t *handle = tparkCreateHandle();

    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tparkWake(handle);
    });

    tparkWait(handle, false);
    tparkDestroyHandle(handle);
    waker.join();

    std::cout << "Thread parked and woken up" << std::endl;
    return 0;
}
```

```cpp
#include <threadpark.h>

#include <iostream>
#include <thread>

int main() {
    tpark_handle_t *handle = tparkCreateHandle();

    std::thread waker([&] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tparkWake(handle);
    });
     
    tparkBeginPark(handle);
    
    // do other stuff here.
    // Even if tparkWake is called before tparkWait, the thread will not "miss" the wake-up.
    // tparkWait will return immediately if the thread was woken up during the section from tparkBeginPark to tparkWait.
    
    tparkWait(handle, true);
    
    tparkEndPark(handle);
    
    tparkDestroyHandle(handle);
    waker.join();

    std::cout << "Thread parked and woken up" << std::endl;
    return 0;
}
```

## Prerequisites

- Git
- CMake (3.22.1 or higher)
- C++ compiler with C++20 support (MSVC 17+, gcc 11+ or clang 12+)

## Supported architectures

threadpark aims to be compatible with all architectures.
Feel free to create issues for architecture-induced compilation failures.

## Building

Below are platform-specific instructions for installing prerequisites and then building threadpark. If your system is not covered or you wish to see how we do it on multiple platforms, refer to our CI configuration.

### Installing prerequisites

In this section we propose methods of installing the required prerequisites for building threadpark on Windows, macOS, Linux, FreeBSD, and OpenBSD.

#### Windows

With the winget package manager installed & up-to-date from the Microsoft Store, you can install the prerequisites as
follows:

```bash
winget install Microsoft.VisualStudio.2022.Community --silent --override "--wait --quiet --add ProductLang En-us --add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended"
winget install Git.Git --source winget
winget install Kitware.CMake --source winget
```

After installing these packages, make sure to refresh your PATH by restarting your explorer.exe in the Task Manager and
opening a new Terminal, launched by said explorer.

#### macOS

```bash
xcode-select --install # if not already installed

# install Homebrew package manager if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

brew install git        # if not already installed by xcode command line tools
brew install cmake
```

#### Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential
sudo apt install -y git
sudo apt install -y cmake
```

#### FreeBSD

```bash
# Update pkg repository and install prerequisites
sudo pkg update -f
sudo pkg install -y cmake llvm16

# Ensure the newer clang is first in your PATH
export PATH="/usr/local/llvm16/bin:$PATH"
```

#### OpenBSD

```bash
# Set up appropriate PKG_PATH for your OpenBSD release (example shown for 7.6)
export PKG_PATH="https://cdn.openbsd.org/pub/OpenBSD/7.6/packages/amd64/"

# Install prerequisites (version numbers may differ)
pkg_add -I cmake-3.30.1v1 llvm-16.0.6p30

# Ensure the newer clang is first in your PATH
export PATH="/usr/local/llvm/bin:$PATH"
```

### Building the native library & other targets

To build all native targets, run the following commands valid for both Windows with PowerShell (replace backslashes if needed) and Unix-like systems (Linux, macOS, FreeBSD, OpenBSD), starting from the root directory of the repository:

```bash
git submodule update --init --recursive
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

If you have multiple C/C++ compilers installed (for instance, a newer LLVM) and wish to make sure CMake uses them, you can specify them explicitly:

```bash
cmake -DCMAKE_C_COMPILER=cc -DCMAKE_CXX_COMPILER=c++ -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

### Recommended way to use the threadpark library

The recommended way to use threadpark in a C/C++ project is to clone the repository and link against the threadpark library in
CMake:

```bash
git clone --recurse https://github.com/PrimeIntellect-ai/threadpark.git
```

Then add the newly cloned repository as a subdirectory in your CMakeLists file:

```cmake
add_subdirectory(threadpark)
```

Then link against the threadpark library

```cmake
target_link_libraries(YourTarget PRIVATE threadpark)
```

## Testing

### C++ Tests

To run the C++ unit tests, starting from the root directory of the repository, run the following commands valid for both
Windows with PowerShell and Unix-like systems:

```bash
cd build
ctest --verbose --build-config Release --output-on-failure
```

## License

This project is licensed under the MIT License.

## Contributing

Contributions are welcome! Please submit issues and pull requests to help improve threadpark.
