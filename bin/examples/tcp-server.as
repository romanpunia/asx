import from { "schedule", "network" };

class socket_event
{
    socket@ connection;

    socket_event(socket@ fd)
    {
        @connection = fd;
    }
    void dispatch()
    {
        try
        {
            /*
                Process incoming requests until we get a
                network error (e.g. reset, timeout, abort) or
                until scheduler becomes inactive.

                This server will respond correctly only for
                incoming HTTP GET requests without a body that
                are under 4kb size.
            */
            schedule@ queue = schedule::get();
            while (queue.is_active())
            {
                /* Read incoming request until HTTP delimiter token (unused) */
                string request_message = co_await connection.read_until_chunked_deferred("\r\n\r\n", 1024 * 4);

                /* Generate a response for a client */
                string response_content = "Hello, World!";
                string response_message = "HTTP/1.1 200 OK\r\n";
                response_message += "Connection: Keep-Alive\r\n";
                response_message += "Keep-Alive: timeout=5\r\n";
                response_message += "Content-Type: text/plain; charset=utf-8\r\n";
                response_message += "Content-Length: " + to_string(response_content.size()) + "\r\n\r\n";
                response_message += response_content;

                /* Send generated response to client */
                co_await connection.write_deferred(response_message);
            }
        }
        catch { }

        /* Close connection if possible */
        try { co_await connection.close_deferred(); } catch { }
    }
}

[#console::main]
[#schedule::main]
int main()
{
    /* Start resolving network events */
    dns@ resolver = dns::get();
    multiplexer@ dispatcher = multiplexer::get();
    dispatcher.activate();

    /*
        Configure a listener socket and start accepting
        new connections. Functions open/bind/listen are
        the standard socket listener flow. Function
        set_blocking enables non-blocking IO. Function
        accept_deferred triggers passed callback each time a
        connection was made.

        We get the socket_address from DNS resolver. It's a
        bit verbose but works good enough. Most functions here
        can throw exceptions.

        This is what happens under the hood of http::server
        on the lowest level, basically. Of course, it will be
        slower than using http::server directly, there is an
        overhead on native function calls which are used here
        all over the place but most of the heavy stuff is in
        event loop that does not allow parallel execution by
        design.
    */
    socket_address address = resolver.lookup("0.0.0.0", "8080", dns_type::listen);
    socket@ listener = socket();
    listener.open(address);
    listener.bind(address);
    listener.listen(128);
    listener.set_blocking(false);

    /*
        This way is the simplest for graceful shutdown,
        when we receive a shutdown signal we will perform
        a few actions needed for proper termination.
    */
    this_process::before_exit(function(signal)
    {
        /*
            Reset all socket connections: multiplexer
            keeps track of all sockets that are awaiting
            IO operations to finish.
        */
        multiplexer::get().shutdown();
    });
    
    /* Process incoming connections until we terminate */
    schedule@ queue = schedule::get();
    while (queue.is_active())
    {
        try
        {
            /* Try to accept next incoming socket */
            socket_accept incoming = co_await listener.accept_deferred();
            
            /* Configure connected client connection */
            socket@ connection = socket(incoming.fd);
            connection.set_io_timeout(5000);
            connection.set_blocking(false);
            connection.set_keep_alive(true);
            
            /*
                Initiate an immediate task which can help us
                run multiple "white true" loops simultaneously.
            */
            socket_event@ event = socket_event(@connection);
            queue.set_immediate(task_parallel(event.dispatch));
        }
        catch
        {
            /* Program can exit now */
            queue.stop();
            break;
        }
    }

    return 0;
}