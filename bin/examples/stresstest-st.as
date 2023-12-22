/*
    This is a simple test that will load a single core
    by computing simple integer hash in a loop. This
    example is an illustration of single core CPU utilisation.
*/
import from "console";

int32 test(int32 value, int32 index)
{
    int32 hash = 0, max = 2 << 29;
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

    int32 index = to_int32(args[args.size() - 1]);
    if (index <= 0)
    {
        term.write_line("invalid test sequence index");
        term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
        return 2;
    }

    string value = to_string(test(index, 0));
    term.write_line("result: " + value);
    term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
    return 0;
}