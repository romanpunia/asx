#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>

int main()
{
    console@ output = console::get();
    schedule@ queue = schedule::get();
    queue.start(schedule_policy(2));
    
    remote_host address;
    address.hostname = "jsonplaceholder.typicode.com";
    address.port = 443;
    address.secure = true;

    http::client@ client = http::client(5000);
    if ((co_await client.connect(address)) < 0)
    {
        output.write_line("cannot connect to remote server");
        queue.stop();
        return 1;
    }

    http::request_frame request;
    request.uri = "/posts/1";

    if (!(co_await client.send(request)))
    {
        output.write_line("cannot receive response from remote server");
        queue.stop();
        return 2;
    }

    if (!client.response.is_ok() || !(co_await client.consume()))
    {
        output.write_line("response from remote server was not successful");
        queue.stop();
        return 3;
    }

    output.write_line(client.response.content.get_text());
    co_await client.disconnect(); // If forgotten then connection will be hard reset
    queue.stop();
    return 0;
}