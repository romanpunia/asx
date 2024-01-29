import from { "schedule", "http", "console", "os" };

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
        /* Set content type of text */
        base.response.set_header("content-type", "text/plain");

        /* Set content text message */
        base.response.content.assign("Hello, World!");

        /* Return result */
        base.finish(200);
    });
    site.post("/upload", function(http::connection@ base)
    {
        /* Prepare and store all possible files to local "temp" directory */
        http::resource_info[]@ resources = co_await base.store();

        /* Generate directory name using macro */
        string directory = __DIRECTORY__ + "/web/";
        try
        {
            /* Create a "web" directory in directory of this file */
            os::directory::patch(directory);

            for (usize i = 0; i < resources.size(); i++)
            {
                http::resource_info next = resources[i];

                /* Move from "temp" to "web" directory */
                os::file::move(next.path, directory + next.name);
            }
        }
        catch { }
        /* Finish with success */
        base.finish(204);
    });

    @server = http::server();
    server.configure(@router);
    server.listen();
    return 0;
}