#include <std/console.as>

/* Contains a simple script with [atob, btoa, from_hex, to_hex] functions */
#include "@avgpythonenjoyer/atob"

int main()
{
    console@ output = console::get();
    string value = "Hello, World!";
    string b64_encoded = btoa(value);
    string b64_decoded = atob(b64_encoded);
    string hex_encoded = to_hex(value);
    string hex_decoded = from_hex(hex_encoded);

    output.write_line("basic value: " + value);
    output.write_line("b64 encoded: " + b64_encoded);
    output.write_line("b64 decoded: " + b64_decoded);
    output.write_line("hex encoded: " + hex_encoded);
    output.write_line("hex decoded: " + hex_decoded);
    return 0;
}