#include <std/schedule.as>
#include <std/promise.as>
#include <std/timestamp.as>
#include <std/console.as>
#include <std/schema.as>

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
    queue.set_timeout(timeout_ms, task_event(task.settle));
    
    console@ output = console::get();
    output.write_line("[timeout] -> " + to_string(timeout_ms) + "ms");
    return task.data;
}
promise<string>@ get_prices_json()
{
    schema@ packages =
    {
        { "week", "3.99" },
        { "month", "14.99" },
        { "year", "139.99" }
    };
    
    co_await set_timeout(500);
    promise<string>@ result = promise<string>();
    result.wrap(packages.to_json());
    return @result;
}

int main()
{
    console@ output = console::get();
    output.show();
    output.write_line("test start");

    schedule@ queue = schedule::get();
    queue.start(schedule_policy(4));

    auto start = timestamp().milliseconds();
    string data = co_await get_prices_json();
    output.write_line("[response] -> " + data);
    
    co_await set_timeout(1000);
    co_await set_timeout(500);
    co_await set_timeout(1000);

    auto end = timestamp().milliseconds();
    output.write_line("test time: " + to_string(end - start) + "ms");
    output.write_line("test end");
    queue.stop();

    return 0;
}