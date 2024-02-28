import from { "schedule", "http", "console", "os" };

http::server@ server = null;

[#console::main]
[#schedule::main]
int main()
{
    http::map_router@ router = http::map_router();
    router.listen("0.0.0.0", "8080");
    router.get("/", function(http::connection@ base)
    {
        /* Set content type of text */
        base.response.set_header("content-type", "text/plain");

        /* Set content text message */
        base.response.content.assign("Hello, World!");

        /* Build, send and finalize result */
        base.next(200);
    });
    router.get("/chunked", function(http::connection@ base)
    {
        /* Set content type of text */
        base.response.set_header("content-type", "text/plain");
        
        try
        {
            /* Send headers for 200 response, set chunked transfer encoding (if not specified not to do this) */
            co_await base.send_headers(200);

            /* Send next chunk */
            for (usize i = 0; i < 1024; i++)
                co_await base.send_chunk("chunk: " + to_string(i + 1) + "\n");
            
            /* Send last chunk */
            co_await base.send_chunk(string());
        }
        catch { }
        
        /* Finalize result */
        base.next();
    });
    router.post("/fetch", function(http::connection@ base)
    {
        /* Set content type from request */
        base.response.set_header("content-type", base.request.get_header("content-type"));

        /* Set content text from request */
        base.response.content.assign(co_await base.fetch());

        /* Build, send and finalize result */
        base.next(200);
    });
    router.post("/store", function(http::connection@ base)
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

        /* Build, send and finalize result */
        base.next(204);
    });

    /*
        Server is global to extend it's lifetime,
        otherwise GC will destroy it at the end of
        the main function. If we also specify some
        "while true" statement then it can be local
    */
    @server = http::server();
    server.configure(@router);
    server.listen();
    return 0;
}