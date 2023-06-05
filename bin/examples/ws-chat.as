#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>
#include <std/mutex.as>
#include <std/crypto.as>
#include <std/codec.as>

class ws_client
{
    http::websocket_frame@ socket;
    string name;

    ws_client(http::websocket_frame@ new_socket)
    {
        @socket = new_socket;
        name = codec::hex_encode(crypto::random_bytes(4));
    }
    bool is_matching(http::websocket_frame@ other_socket)
    {
        return socket is other_socket;
    }
}

http::server@ server = null;
mutex@ section = mutex();
console@ output = console::get();
ws_client@[] clients;

ws_client@ get_client(http::websocket_frame@ base, bool erase)
{
    // Could be implemented through dictionary for larger example
    section.lock();
    for (usize i = 0; i < clients.size(); i++)
    {
        auto@ next = clients[i];
        if (next.is_matching(@base))
        {
            if (erase)
                clients.remove_at(i);

            section.unlock();
            return next;
        }
    }
    section.unlock();
    return null;
}
void add_client(http::websocket_frame@ base)
{
    ws_client@ client = ws_client(@base);
    /* Might be executed in parallel, mutex needed. */
    section.lock();
    clients.push(@client);
    section.unlock();

    /* Send a notification to other clients */
    broadcast_client(@base, client.name + " has connected", http::websocket_op::text);
}
void broadcast_client(http::websocket_frame@ base, const string&in data, http::websocket_op type)
{
    /* Log the message */
    output.write_line(data);

    promise<bool>@[] targets;
    {
        /* Might also be executed in parallel, mutex needed. */
        section.lock();
        targets.reserve(clients.size());
        for (usize i = 0; i < clients.size(); i++)
        {
            auto@ next = clients[i];
            if (!next.is_matching(@base))
                targets.push(next.socket.send(data, type));
        }
        section.unlock();
    }

    /* We must not co_await while holding the mutex, that's prohibited. */
    for (usize i = 0; i < targets.size(); i++)
        co_await targets[i];
}
void remove_client(http::websocket_frame@ base)
{
    ws_client@ client = get_client(@base, true);
    if (client is null)
        return;

    /* Send a notification to other clients */
    broadcast_client(@base, client.name + " has disconnected", http::websocket_op::text);
}
void exit_main()
{
    server.unlisten(1);
    schedule::get().stop();
}
int main()
{
    console@ output = console::get();
    schedule_policy policy;
    schedule@ queue = schedule::get();
    queue.start(policy);
    
    http::map_router@ router = http::map_router();
    router.listen("127.0.0.1", 8080);
    
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
        /* Execute if we have any data */
        if (type == http::websocket_op::text || type == http::websocket_op::binary)
        {
            auto@ client = get_client(@base, false);
            if (client !is null)
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
    return 0;
}