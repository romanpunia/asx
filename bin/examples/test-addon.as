#include <std/console.as>
#include "../../var/test_addon/bin/test_addon"

/* to test this example you must build the subproject at var/test_addon */
int main()
{
    auto@ output = console::get();
    print_hello_world();
    output.write_line(get_test_memory());
    output.read(8);
    return 0;
}