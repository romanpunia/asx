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
        if (!(co_await client.connect(address)))
            throw exception_ptr("connect", "cannot connect to remote server");

        /* send request data */
        http::request_frame request;
        request.uri = "/posts/1";
        if (!(co_await client.send(request)) || !client.response.is_ok())
            throw exception_ptr("send", "response was not successful (code = " + to_string(client.response.status_code) + ")");

        /* fetch response content */
        if (!(co_await client.consume()))
            throw exception_ptr("consume", "cannot fetch data from response");

        output.write_line(client.response.content.get_text());
    }
    catch
    {
        auto error = exception::unwrap();
        output.write_line(error.what());
    }
    
    co_await client.disconnect(); // If forgotten then connection will be hard reset
    queue.stop();
    return 0;
}