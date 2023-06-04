#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>
#include <std/mutex.as>

http::websocket_frame@[] clients;
http::server@ server = null;
mutex@ section = mutex();

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
    site.get_base().allow_websocket = true;
    site.websocket_connect("/", function(http::websocket_frame@ base)
    {
        /*
            Callback might be executed in parallel, mutex needed.
        */
        section.lock();
        clients.push(@base);
        section.unlock();

        /* Let websocket state machine do it's job */
        base.next();
    });
    site.websocket_receive("/", function(http::websocket_frame@ base, http::websocket_op type, data)
    {
        if (type != http::websocket_op::text && type != http::websocket_op::binary)
            return;

        /*
            Clients array could change before we are done,
            we need to copy it. Also mutex cannot be used with
            co_await, as only one thread can own it at a time,
            if we do ["lock" -> "co_await send" -> "unlock"] we get
            deadlock or system_error as it might lock thread A,
            then unlock thread B that didn't own the mutex in
            the first place.
        */
        section.lock();
        http::websocket_frame@[] targets;
        targets.reserve(clients.size());
        for (usize i = 0; i < clients.size(); i++)
        {
            auto@ next = clients[i];
            if (next !is base) // Don't wanna send the message to myself
                targets.push(@next);
        }

        section.unlock();
        for (usize i = 0; i < targets.size(); i++)
            co_await targets[i].send(data, type); // Could be optimized to initiate send for all, then await all operations
        base.next();
    });
    site.websocket_disconnect("/", function(http::websocket_frame@ base)
    {
        /*
            Remove client from array by comparing memory
            addresses with each of clients.
        */
        section.lock();
        for (usize i = 0; i < clients.size(); i++)
        {
            if (clients[i] is base)
            {
                clients.remove_at(i);
                break;
            }
        }
        section.unlock();
        base.next();
    });

    @server = http::server();
    server.configure(@router);
    server.listen();
    return 0;
}