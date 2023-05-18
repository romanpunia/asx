/*
    Testcase conclusions:
      1. Default Mavi.as is slower than Node.js
      2. JIT version is faster than Node.js (v8 fails to generate proper machine code for this case)
      3. Both Mavi.as and Node.js are faster python2/3 by several magnitudes
      4. This is a good case scenario for Mavi.as JIT compiler

    > vi -v
      v21.27.12
    > vi -f examples/cpu/test 120000000
      ~2351ms
    > vi -j -f examples/cpu/test 120000000
      ~526ms
*/
#include <std/console.as>

int64 test(int64 value)
{
    int64 hash = 0, max = 2 << 29;
    while (value > 0)
        hash = ((hash << 5) - hash + value--) % max;
    return hash;
}
int main(string[]@ args)
{
    auto@ term = console::get();
    term.capture_time();
    if (args.empty())
    {
        term.write_line("provide test sequence index");
        term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
        return 1;
    }

    int64 index = to_int(args[args.size() - 1]);
    if (index <= 0)
    {
        term.write_line("invalid test sequence index");
        term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
        return 2;
    }

    string value = to_string(test(index));
    term.write_line(value);
    term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
    return 0;
}