#include <mavi/core/scripting.h>
#include <mavi/mavi.h>

using namespace Mavi::Core;
using namespace Mavi::Compute;
using namespace Mavi::Scripting;

void PrintHelloWorld()
{
    auto* Output = Console::Get();
    Output->Begin();
    Output->fWriteLine("Mavi v%i.%i.%i", Mavi::Library::GetMajorVersion(), Mavi::Library::GetMinorVersion(), Mavi::Library::GetPatchVersion());
    Output->WriteLine("Hello, World!");
    Output->End();
}
String GetTestMemory()
{
    char* Bytes = VI_MALLOC(char, 32);
    for (size_t i = 0; i < 32; i++)
        Bytes[i] = Crypto::RandomUC();
    
    String Data = Codec::HexEncode(Bytes, 32);
    VI_FREE(Bytes);
    return Data;
}

extern "C" { VI_EXPOSE int ViInitialize(VirtualMachine*); }
int ViInitialize(VirtualMachine* VM)
{
    VM->ImportSubmodule("std/string");
    VM->SetFunction("void print_hello_world()", &PrintHelloWorld);
    VM->SetFunction("string get_test_memory()", &GetTestMemory);
    return 0;
}