import from { "schedule", "network" };

/* For simplicity of listener capturing while being in shutdown event */
socket@ listener = null;

int main()
{
    schedule@ queue = schedule::get();
    queue.start(schedule_policy());

    /* Start resolving network events */
    dns@ resolver = dns::get();
    multiplexer@ dispatcher = multiplexer::get();
    dispatcher.activate();

    /*
        Configure a listener socket and start accepting
        new connections. Functions open/bind/listen are
        are the standard socket listener flow. Function
        set_blocking enables non-blocking IO. Function
        accept_async triggers passed callback each time a
        connection was made.

        We get the socket_address from DNS resolver. It's a
        bit verbose but works good enough. Most functions here
        can throw exceptions.

        This is what happens under the hood of http::server
        on the lowest level, basically.
    */
    socket_address@ address = resolver.from_service("0.0.0.0", "8080", dns_type::listen, socket_protocol::tcp, socket_type::stream);
    @listener = socket();
    listener.open(@address);
    listener.bind(@address);
    listener.listen(128);
    listener.set_blocking(false);
    listener.accept_async(false, function(fd, address)
    {
        /* Configure connected client connection */
        socket@ connection = socket(fd);
        connection.set_io_timeout(5000);
        connection.set_blocking(false);
        connection.set_keep_alive(true);

        try
        {
            /*
                Process incoming requests until we get a
                network error (e.g. reset, timeout, abort) or
                until scheduler becomes inactive.

                This server will respond correctly only for
                incoming HTTP GET requests without a body.
            */
            schedule@ queue = schedule::get();
            while (queue.is_active())
            {
                /* Read incoming request until HTTP delimiter token (unused) */
                string request_message = co_await connection.read_until_chunked(1024 * 4, "\r\n\r\n");

                /* Generate a response for a client */
                string response_content = "Hello, World!";
                string response_message = "HTTP/1.1 200 OK\r\n";
                response_message += "Connection: Keep-Alive\r\n";
                response_message += "Keep-Alive: timeout=5\r\n";
                response_message += "Content-Type: text/plain; charset=utf-8\r\n";
                response_message += "Content-Length: " + to_string(response_content.size()) + "\r\n\r\n";
                response_message += response_content;

                /* Send generated response to client */
                co_await connection.write(response_message);
            }
        }
        catch { }

        try
        {
            /* Close connection if possible */
            co_await connection.close();
        }
        catch { }
    });

    /*
        This way is the simplest for graceful shutdown,
        when we receive a shutdown signal we will perform
        a few actions needed for proper termination.
    */
    this_process::before_exit(function(signal)
    {
        /*
            First we cancel all events on listener which
            otherwise could not be released by GC because
            it is captured by internal network system.

            If for some reason listener socket had a timeout
            value set then multiplexer shutdown would have the
            same effect.
        */
        listener.shutdown(true);

        /* Then we reset all client connections with a timeout */
        multiplexer::get().shutdown();

        /* Program can exit now */
        schedule::get().stop();
    });
    return 0;
}