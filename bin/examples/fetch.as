import from
{
    "schedule",
    "http",
    "console",
    "exception"
};

/*
    Essentially like an http::client but easier though having less control overall,
    unlike streams this type of web request will always by non-blocking.
*/
[#console::main]
[#schedule::main(threads = 1, stop = true)]
int main()
{
    http::fetch_frame options;
    options.set_header("user-agent", "I'm a tester");
    options.max_size = 1024 * 1024; // Up to 1 megabyte of response will be stored in memory

    try
    {
        /* Will throw on network error */
        http::response_frame response = co_await http::fetch("https://jsonplaceholder.typicode.com/posts/1", "GET", options);
        console::get().write_line(response.content.get_text());
    }
    catch
    {
        console::get().write_line(exception::unwrap().what());
    }
    
    return 0;
}