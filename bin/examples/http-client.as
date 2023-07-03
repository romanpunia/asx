#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>

void stop(console@ output, schedule@ queue, http::client@ client)
{
    co_await client.close();
    queue.stop();
    output.read_char();
}
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
        stop(@output, @queue, @client);
        return 1;
    }

    http::request_frame request;
    request.uri = "/posts/1";

    if (!(co_await client.send(request)))
    {
        output.write_line("cannot receive response from remote server");
        stop(@output, @queue, @client);
        return 2;
    }

    if (!client.response.is_ok() || !(co_await client.consume()))
    {
        output.write_line("response from remote server was not successful");
        stop(@output, @queue, @client);
        return 3;
    }

    output.write_line(client.response.content.get_text());
    stop(@output, @queue, @client);
    return 0;
}