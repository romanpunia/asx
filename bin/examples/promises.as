#include <std/schedule.as>
#include <std/promise/async.as>
#include <std/timestamp.as>
#include <std/console.as>

class timeout_event
{
    promise<bool>@ data = promise<bool>();

    void settle()
    {
        console@ output = console::get();
        output.write_line("timer event triggered");
        data.wrap(true);
    }
}

promise<bool>@ set_timeout(uint64 timeout_ms)
{
    timeout_event@ event = timeout_event();
    schedule@ queue = schedule::get();
    queue.set_timeout(timeout_ms, task_event(event.settle), difficulty::light, true);
    return event.data;
}

int main()
{
    console@ output = console::get();
    output.show();
    output.write_line("test start");

    schedule_policy policy;
    policy.set_threads(4);

    schedule@ queue = schedule::get();
    queue.start(policy);

    auto start = timestamp().milliseconds();
    output.write_line("[timeout] 1 -> 1000ms");
    co_await set_timeout(1000);
    
    output.write_line("[timeout] 2 -> 500ms");
    co_await set_timeout(500);

    output.write_line("[timeout] 3 -> 1000ms");
    co_await set_timeout(1000);

    auto end = timestamp().milliseconds();
    output.write_line("test time: " + to_string(end - start) + "ms");
    output.write_line("test end");
    queue.stop();
    output.read(8);

    return 0;
}