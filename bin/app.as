#include "std/console"

int main(string[]@ args)
{
    console@ output = console::get();
    output.writeLine("hello, world!");
    output.read(4);

    return 0;
}