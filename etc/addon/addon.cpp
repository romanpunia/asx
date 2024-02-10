#include <vitex/scripting.h>
#include <iostream>

void PrintHelloWorld()
{
    std::cout << "Hello, World!" << std::endl;
}

extern "C" { VI_EXPOSE int ViInitialize(Vitex::Scripting::VirtualMachine*); }
int ViInitialize(Vitex::Scripting::VirtualMachine* VM)
{
    VM->SetFunction("void print_hello_world()", &PrintHelloWorld);
    return 0;
}

extern "C" { VI_EXPOSE void ViUninitialize(Vitex::Scripting::VirtualMachine*); }
void ViUninitialize(Vitex::Scripting::VirtualMachine* VM)
{
}