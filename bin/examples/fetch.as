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
int main()
{
    console@ output = console::get();
    schedule@ queue = schedule::get();
    queue.start(schedule_policy(2));
    
    http::fetch_frame options;
    options.set_header("user-agent", "I'm a tester");
    options.max_size = 1024 * 1024; // Up to 1 megabyte of response will be stored in memory

    try
    {
        /* Will throw on network error */
        http::response_frame response = co_await http::fetch("https://jsonplaceholder.typicode.com/posts/1", "GET", options);
        output.write_line(response.content.get_text());
    }
    catch
    {
        output.write_line(exception::unwrap().what());
    }
    
    queue.stop();
    return 0;
}