import from { "schedule", "http", "console", "exception" };

int main()
{
    console@ output = console::get();
    schedule@ queue = schedule::get();
    queue.start(schedule_policy(2));
    
    http::client@ client = http::client(5000); // timeout = 5 seconds
    try
    {
        /* connect to server */
        remote_host address;
        address.hostname = "jsonplaceholder.typicode.com";
        address.port = 443;
        address.secure = true;
        co_await client.connect(address);

        for (usize i = 0; i < 3; i++)
        {
            /* send request data and fetch response body */
            http::request_frame request;
            request.location = "/posts/" + to_string(i + 1);

            /* before sending another request we must fetch response body */
            co_await client.send_fetch(request);
            
            /* display response body */
            output.write_line(client.response.content.get_text());
        }
    }
    catch
    {
        output.write_line(exception::unwrap().what());
    }
    
    try
    {
        co_await client.disconnect(); // If forgotten then connection will be hard reset, throws if already disconnected
    }
    catch { }

    queue.stop();
    return 0;
}