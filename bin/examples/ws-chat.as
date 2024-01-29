import from
{
    "schedule",
    "http",
    "console",
    "crypto",
    "codec",
    "dictionary"
};

class ws_client
{
    http::websocket_frame@ socket;
    string id;
    string name;

    ws_client(http::websocket_frame@ new_socket)
    {
        @socket = new_socket;
        id = client_id(@socket);
        name = codec::hex_encode(crypto::random_bytes(4));
    }
}

/*
    This example is multithreaded, however as it is ran with an
    event loop, access to @clients variable is synchronized. Although,
    using co_await might shuffle things around so we don't loop this
    dictionary with coroutines instead we just initiate all operations
    at the same time then we await them. That's why we don't need the mutex.

    Dictionary is used because we don't have traditional hash set for object
    handles. We convert object handle (pointer) address to string and then use
    it as a hash key for dictionary.
*/
http::server@ server = null;
console@ output = console::get();
dictionary@ clients = dictionary();

string client_id(http::websocket_frame@ base)
{
    /* Converts object handle to websocket object to unique (to this handle) hex string */
    return handle_id(to_ptr(@base));
}
void add_client(http::websocket_frame@ base)
{
    /* Create a new client state and push it to dictionary */
    ws_client@ client = ws_client(@base);
    clients.set(client.id, @client);

    /* Send a notification to other clients */
    broadcast_client(null, client.name + " has connected", http::websocket_op::text);
}
void broadcast_client(http::websocket_frame@ base, const string&in data, http::websocket_op type)
{
    /* Log the message */
    output.write_line(data);

    /* Send message to all the clients except one that sent it */
    promise<bool>@[] broadcasts;
    broadcasts.reserve(clients.size());
    for (usize i = 0; i < clients.size(); i++)
    {
        /*
            Dictionary has two index operators:
                [string] = get by key,
                [usize] = get by index
            operator returns storable object that
            should be casted to needed type
        */
        ws_client@ next = cast<ws_client@>(clients[i]);
        if (next.socket !is base)
            broadcasts.push(next.socket.send(data, type));
    }

    /* We do that because while we await for promises other code might modify our clients container */
    for (usize i = 0; i < broadcasts.size(); i++)
        co_await broadcasts[i];
}
void remove_client(http::websocket_frame@ base)
{
    /* Remove this client and send a notification to other clients */
    ws_client@ client = null;
    if (clients.get(client_id(@base), @client))
    {
        broadcast_client(@base, client.name + " has disconnected", http::websocket_op::text);
        clients.erase(client.id);
    }
}
int main()
{
    console@ output = console::get();
    schedule_policy policy;
    schedule@ queue = schedule::get();
    queue.start(policy);
    
    http::map_router@ router = http::map_router();
    router.listen("0.0.0.0", 8080);
    
    http::site_entry@ site = router.site("*");
    site.websocket_connect("/", function(http::websocket_frame@ base)
    {
        /* Add client and notify others */
        add_client(@base);

        /* Let websocket state machine do it's job */
        base.next();
    });
    site.websocket_receive("/", function(http::websocket_frame@ base, http::websocket_op type, data)
    {
        /* Send to others if we have any data */
        if (type == http::websocket_op::text || type == http::websocket_op::binary)
        {
            ws_client@ client = null;
            if (clients.get(client_id(@base), @client))
                broadcast_client(@base, client.name + ": " + data, type);
        }

        base.next();
    });
    site.websocket_disconnect("/", function(http::websocket_frame@ base)
    {
        /* Remove client and notify others */
        remove_client(@base);
        base.next();
    });

    http::route_entry@ route = site.get_base();
    route.websocket_timeout = 0; // By default 30s of inactivity will result in disconnect (zero = no timeout)
    route.allow_websocket = true; // Disabled by default

    @server = http::server();
    server.configure(@router);
    server.listen();

    /*
        Graceful shutdown: by default server
        will be automatically destroyed by GC,
        however in this case we might get null
        pointer exception because while destroying
        the server we might get "remove_client" callback
        which will use "clients" global variable
        which could be null because it might get GC'd
        before the server instance.
    */
    this_process::before_exit(function(signal)
    {
        /* Wait at most 1 second for connections to shutdown */
        server.unlisten(1);

        /* Explicitly state that we are done processing and runtime can safely exit */
        schedule::get().stop();
    });
    return 0;
}