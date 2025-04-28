#include <iostream>
#include "interface.hpp"

void print_hello_world()
{
    std::cout << "Hello, world!" << std::endl;
}

extern "C" { INTERFACE_EXPORT int addon_import(); }
int addon_import()
{
    if (!asx_import_interface())
        return -1;

    asx_export_function("void print_hello_world()", &print_hello_world);
    return 0;
}

extern "C" { INTERFACE_EXPORT void addon_cleanup(); }
void addon_cleanup() { }