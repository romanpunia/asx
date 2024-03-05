## Build: From scratch
_Note: there is a number of dependencies that must be installed in order to build successfully, exact name of each dependency is very platform dependent. You may pass several Vitex based flags to CMake to disable them, they are listed in the official Vitex repository. While Vitex presents them as features, this project requires them as dependencies._

Clone this repository recursively
```bash
git clone https://github.com/romanpunia/asx --recursive
```

Generate and build project files while being inside of repository
```bash
cmake . -DCMAKE_BUILD_TYPE=Release # -DVI_CXX=17
```
Build project files while being inside of repository
```bash
cmake --build . --config Release
```