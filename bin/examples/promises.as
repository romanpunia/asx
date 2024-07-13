import from
{
    "schedule",
    "promise",
    "timestamp",
    "console",
    "schema",
    "exception"
};

class timeout_task
{
    promise<void>@ data = promise<void>();
    uint64 time;

    void settle()
    {
        console@ output = console::get();
        output.write_line("[event] triggered -> " + to_string(time) + "ms");
        data.wrap();
    }
}

promise<void>@ set_timeout(uint64 timeout_ms)
{
    timeout_task@ task = timeout_task();
    task.time = timeout_ms;

    schedule@ queue = schedule::get();
    queue.set_timeout(timeout_ms, task_async(task.settle));
    
    console@ output = console::get();
    output.write_line("[timeout] -> " + to_string(timeout_ms) + "ms");
    return task.data;
}
promise<string>@ set_exception_always()
{
    /* Simulate some work */
    co_await set_timeout(500);
    
    /* Create a pending promise */
    promise<string>@ result = promise<string>();

    /* Reject a pending promise */
    result.except(exception_ptr("fetch", "cannot receive a valid response (expected)"));
    return @result;
}
promise<string>@ get_prices_json()
{
    /* Simulate some work */
    co_await set_timeout(500);

    /* Create a pending promise */
    promise<string>@ result = promise<string>();

    /* Settle a pending promise */
    result.wrap(schema(
    {
        { "week", "3.99" },
        { "month", "14.99" },
        { "year", "139.99" }
    }).to_json());
    return @result;
}

/*
    Unlike JavaScript or other languages here, in AngelScript,
    you always can call co_await because every single function
    call is performed in a vm context that is always ready to
    be suspended. This can be seen as weekness sometimes as we
    can't truly tell if function is async or not. However, if
    we accept that, then it may be treated as an opportunity to
    just never think about it.

    In cases where async functionality will really be unnecessary,
    it will be disabled, for example, array.sort is not a place where
    we would await for some async operation. All so called "subcalls"
    are executing without coroutine support. If you are binding a callback
    that will be executed later then it will support async, if you call a
    function that will always call your callback inside this function then
    it will not support async (exception will be thrown).
*/
[#console::main]
[#schedule::main(threads = 1, stop = true)]
int main()
{
    console@ output = console::get();
    output.write_line("test start");

    auto start = timestamp().milliseconds();
    {
        string json_data = co_await get_prices_json();
        output.write_line("[response:first] -> OK " + json_data);
        
        try
        {
            string exception_data = co_await set_exception_always();
            output.write_line("[response:second] -> OK " + exception_data);
        }
        catch
        {
            output.write_line("[response:second] -> ERR " + exception::unwrap().what());
        }

        try
        {
            /* Create and unwrap a pending promise (not allowed when not ready) */
            string invalid_result = promise<string>().unwrap();
            output.write_line("[response:third] -> OK " + invalid_result);
        }
        catch
        {
            output.write_line("[response:third] -> ERR " + exception::unwrap().what() + " (also expected)");
        }
    }
    auto end = timestamp().milliseconds();
    output.write_line("test time: " + to_string(end - start) + "ms");
    output.write_line("test end");
    return 0;
}