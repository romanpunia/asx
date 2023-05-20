<br/>
<div align="center">
    <br />
    <img src="https://github.com/romanpunia/mavi/blob/master/var/logo.png?raw=true" alt="Mavi Logo" width="300" />
    <h3>Angel Script Runtime Environment</h3>
</div>

## About
Mavi.as is a fully featured Angel Script environment similar to Node.js.

## Usage
Show all commands: **vi -h** or **vi --help**

Execute a script file: **vi -f [path] [args]**

Debug a script file: **vi -d -f [path] [args]**

Run in interactive mode: **vi** or **vi -i**

## Preprocessor
Scripts support preprocessor that can be used just like C++ preprocessor for dependency management. Preprocessed script lines of code could get shuffled around, however if compile or runtime error happens you will get last N lines of source code where it happend (including column pointer).
```cpp
// Global include search (built-in addons, can be disabled)
#include <std/console.as> // Standard library include
#include <std/console> // Shorter version

// Local include search (any script file, can be disabled)
#include "file" // Short include version
#include "file.as" // Will include file at current directory
#include "./file.as" // Same but more verbose
#include "./../file.as" // Will include file from parent directory
#include "../file.as" // Same but less verbose

// Global or local addon search (C++ addons, can be disabled)
#include <addon.so> // Linux SO (global search will be used)
#include <addon.dylib> // Mac SO
#include <addon.dll> // Windows SO
#include <addon> // Or automatic search

// Remote includes (local only syntax, can be disabled)
#include "https://raw.githubusercontent.com/romanpunia/mavi.as/main/bin/examples/utils/win32.as" // A file from remote server
```

Scripts are written using Angel Script syntax as usual. Standard library defers from default provided addons at angelcode. Entrypoint is defined by either _\<int main()\>_ or _\<int main(string[]@)\>_ function signatures.
```cpp
#include <std/string.as> // By default string class is not exposed

int main() { return 0; }
int main(string[]@ args) { return 0; }
```

Macros are also supported.
```cpp
#define SUM(a, b) ((a) + (b))

int main()
{
#ifdef SUM
    return SUM(1, 2);
#else
    return 0;
#endif
}
```

Preprocessor also supports shared object imports. They are not considered addons or plugins in any way. They can be used to implement some low level functionality without accessing C++ code. More on that in **bin/examples/utils/win32.as**.
```cpp
#pragma cimport("kernel32.dll", "GetCurrentProcessId", "uint32 win32_get_pid()") // SO filename or path, function name to find in SO, function definition to expose to Angel Script.

int main()
{
#ifdef SOF_GetCurrentProcessId // SOF = shared object function, SOF_* will be defined if function has successfully been exposed
    return win32_get_pid();
#else
    return 0;
#endif
}
```

## Addons
There is support for addons. Addons must be compiled with Mavi as a shared object dependency or they must load Mavi symbols manually to work properly. Addon is a C++ shared library that implements following methods:
```cpp
#include <mavi/core/scripting.h>

extern "C" { VI_EXPOSE int ViInitialize(Mavi::Scripting::VirtualMachine*); }
int ViInitialize(Mavi::Scripting::VirtualMachine* VM) // Required initialization for requested virtual machine
{
    return 0; // Zero is successful initialization
}

extern "C" { VI_EXPOSE int ViUninitialize(Mavi::Scripting::VirtualMachine*); }
void ViUninitialize(Mavi::Scripting::VirtualMachine* VM) // Optional deinitialization for requested virtual machine
{
}
```
You can find an example addon project in _/var/test_addon_ directory, it includes CMake list file and C++ source file. You can test it executing **bin/examples/addons.as**.

## Debugging
You may just run mavi with _--debug_ or _-d_ flag. This will allocate resources for debugger context and before executing anything it will debug-stop. Type _help_ to view available commands.

## Performance
Although AngelScript is pretty fast out of the box (default tuned about 2-6x faster python3), if code being executed is CPU intensive and lies within VM then it might be useful to run it using JIT compiler. This compiler is pretty old, somewhat unstable and does not support ARM platform, it can actually work only on Windows and Linux. Nevertheless, it's still pretty powerful and can increase performance by 4 orders of magnitude. Add _--jit_ option to enable it. You may also check comparisons in **bin/examples/cpu/test.\***.

Currently, one of the main issue is initialization time. About 40ms (app-mode) or 210ms (game-mode) of time is taken by initialization that does not include script source code compilation or execution. However it does not mean this time will grow as dramatically as Node.js initialization time when loading many CommonJS modules.

## Memory usage
Generally, AngelScript uses much less memory than v8 JavaScript runtime. That is because there are practically no wrappers between C++ types and AngelScript types. However, JIT compiler may increase memory usage as well as source code preserving in memory.

## Dependencies
* [Mavi (submodule)](https://github.com/romanpunia/mavi)

## Building
To build this project you have to clone Mavi, also you need to make sure that it can be build on your machine. CMake's **VI_DIRECTORY** is a path to Mavi source folder.

## License
Mavi.as is licensed under the MIT license