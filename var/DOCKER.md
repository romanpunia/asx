
## Build: As a Docker image
_Note: Docker images are not optimized for size, resulting image will be over-bloated_

There are example docker files that can be used to build this project with all optional dependencies installed. Resulting files will be installed to following paths:
```sh
/usr/local/bin
/usr/local/lib
```

Clone this repository recursively
```bash
git clone https://github.com/romanpunia/asx --recursive
```

To customize your build you may use following docker build-arg arguments:
```sh
$CONFIGURE # CMake configuration arguments
$COMPILE   # Compiler configuration arguments
```

### Image: GCC Ubuntu
```sh
docker build -f var/images/gcc.Dockerfile -t asx-gcc:staging .
```

### Image: LLVM Debian
```sh
docker build -f var/images/llvm.Dockerfile -t asx-llvm:staging .
```