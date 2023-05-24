#include <std/console.as>
#include "test_addon/bin/test_addon"

/*
    To test this example you must:
        1. Call: vi -n test_addon -ia examples
        2. Build project at ./test_addon using CMake
*/
int main()
{
    print_hello_world();
    return 0;
}