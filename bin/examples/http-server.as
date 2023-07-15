#include <std/schedule.as>
#include <std/http.as>
#include <std/console.as>

http::server@ server = null;

int main()
{
    console@ output = console::get();
    output.show();
    
    schedule@ queue = schedule::get();
    queue.start(schedule_policy()); // Creates up to "CPU threads count" threads
    
    http::map_router@ router = http::map_router();
    router.listen("0.0.0.0", 8080);
    
    http::site_entry@ site = router.site("*");
    site.get("/", function(http::connection@ base)
    {
        base.response.set_header("content-type", "text/plain");
        base.response.content.assign("Hello, World!");
        base.finish(200);
    });
    site.get("/echo", function(http::connection@ base)
    {
        string content = co_await base.consume();
        base.response.set_header("content-type", "text/plain");
        base.response.content.assign(content);
        base.finish(200);
    });

    @server = http::server();
    server.configure(@router);
    server.listen();

    at_exit(function(signal)
    {
        server.unlisten(1);
        schedule::get().stop();
    });
    return 0;
}