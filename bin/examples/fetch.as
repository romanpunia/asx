#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>

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
    options.set_header("user-agent", "John Roth (The Hangman)");
    options.max_size = 1024 * 1024; // Up to 1 megabyte of response will be stored in memory

    http::response_frame response = co_await http::fetch("https://jsonplaceholder.typicode.com/posts/1", "GET", options);
    queue.stop();

    if (!response.is_ok())
    {
        if (response.is_undefined())
            output.write_line("cannot connect to remote server");
        else
            output.write_line("response from remote server was not successful");
        return 1;
    }

    output.write_line(response.content.get_text());
    output.read(1);
    return 0;
}