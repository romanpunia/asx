#include <std/console.as>
#include <std/schema.as>
#include <std/os.as>

int main(string[]@ args)
{
    console@ output = console::get();
    output.show();

    if (args.size() < 2)
    {
        output.write_line("provide a valid path or URL that should be fetched");
        return 1;
    }

    string text = os::file::read_as_string(args[1]);
    if (args.size() < 3)
    {
        output.write_line(text);
        return 0;
    }
    
    schema@ data = schema::from_json(text);
    if (data is null)
        @data = schema::from_xml(text);
    
    if (data is null)
    {
        output.write_line("data is not a JSON or XML");
        return 2;
    }

    schema@ target = data.get(args[2]);
    if (target is null)
    {
        output.write_line("data does not contain \"" + args[2] + "\" path");
        return 3;
    }

    output.write_line(target.to_string());
    return 0;
}