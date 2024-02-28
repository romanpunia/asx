import from { "console", "schema", "os", "exception" };

[#console::main]
int main(string[]@ args)
{
    console@ output = console::get();
    if (args.size() < 2)
    {
        output.write_line("provide a valid path or URL that should be fetched");
        return 1;
    }

    try
    {
        output.write_line(os::file::read_as_string(args[1]));
        return 0;
    }
    catch
    {
        output.write_line(exception::unwrap().what());
        return 1;
    }
}