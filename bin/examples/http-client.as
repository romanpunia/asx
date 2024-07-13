import from { "schedule", "http", "console", "exception" };

[#console::main]
[#schedule::main(threads = 1, stop = true)]
int main()
{
    http::client@ client = http::client(5000); // timeout = 5 seconds
    try
    {
        /* resolve the address */
        socket_address address = co_await dns::get().lookup_deferred("jsonplaceholder.typicode.com", "443", dns_type::connect);

        /* connect to server using async io and verify up to 100 tls peers (default) */
        co_await client.connect_async(address, 100);

        for (usize i = 0; i < 3; i++)
        {
            /* send request data and fetch response body */
            http::request_frame request;
            request.location = "/posts/" + to_string(i + 1);

            /* before sending another request we must fetch response body */
            co_await client.send_fetch(request);
            
            /* display response body */
            console::get().write_line(client.response.content.get_text());
        }
    }
    catch
    {
        console::get().write_line(exception::unwrap().what());
    }
    
    try
    {
        co_await client.disconnect(); // If forgotten then connection will be hard reset, throws if already disconnected
    }
    catch { }
    return 0;
}