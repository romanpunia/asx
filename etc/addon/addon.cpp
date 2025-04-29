#include <iostream>
#include "interface.hpp"

void print_hello_world()
{
    std::cout << "Hello, world!" << std::endl;
}

extern "C" { INTERFACE_EXPORT int addon_import(); }
int addon_import()
{
    asx_import_interface();
    asx_export_function("void print_hello_world()", &print_hello_world);
    return 0;
}

extern "C" { INTERFACE_EXPORT void addon_cleanup(); }
void addon_cleanup() { }