#include <vitex/scripting.h>
#include <iostream>
#if defined(_WIN32) || defined(_WIN64)
#define VI_EXPOSE __declspec(dllexport)
#else
#define VI_EXPOSE
#endif

void print_hello_world()
{
    std::cout << "Hello, world!" << std::endl;
}

extern "C" { VI_EXPOSE int vi_initialize(vitex::scripting::virtual_machine*); }
int vi_initialize(vitex::scripting::virtual_machine* vm)
{
    vm->set_function("void print_hello_world()", &print_hello_world);
    return 0;
}

extern "C" { VI_EXPOSE void vi_uninitialize(vitex::scripting::virtual_machine*); }
void vi_uninitialize(vitex::scripting::virtual_machine* vm)
{
}