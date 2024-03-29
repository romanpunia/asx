import from
{
    "thread",
    "mutex",
    "console",
    "crypto"
};

string[] ids = string[]();
mutex@ mut = mutex();

[#console::main]
int main()
{
    console@ output = console::get();
    output.write_line("main thread: " + this_thread::get_id());

    /*
        These thread objects are native OS threads,
        they are heavy however fully detached from main
        thread. Each thread has shared access to global
        variables and singletons (meaning to all shared memory).
        This gives great control and very much the easiest way
        to get a segmentation fault. Rules are just like in C++:
        use mutexes or other synchronization primitives.

        There is another way to approach multithreading: use
        schedule::spawn method. When scheduling runtime is active
        this method will create a new execution context that will
        be fully separate from main context. However it will not
        create any new threads, it will only reuse existing ones.
    */
    thread@ basic_parallel = thread(function(thread@ self)
    {
        console::get().write_line("basic parallel thread: " + self.get_id());
        this_thread::sleep(100);
    });
    thread@ shared_data = thread(function(thread@ self)
    {
        console@ output = console::get();
        output.write_line("shared data thread: " + self.get_id());
        mut.lock();
        output.write_line("sdt: push " + self.get_id());
        ids.push(self.get_id());
        mut.unlock();
        this_thread::sleep(100);
    });
    thread@ channel_data = thread(function(thread@ self)
    {
        console@ output = console::get();
        output.write_line("channel data thread: " + self.get_id());

        string value;
        while (self.pop(value))
        {
            this_thread::sleep(100);
            output.write_line("cdt: pop " + value);
            if (value.empty())
                break;
        }
        this_thread::sleep(100);
    });
    thread@ coroutine = thread(function(thread@ self)
    {
        console@ output = console::get();
        output.write_line("coroutine thread: " + self.get_id());
        output.write_line("ct: suspend 1");
        self.suspend();
        this_thread::sleep(500);
        output.write_line("ct: resume 1");
        output.write_line("ct: suspend 2");
        self.suspend();
        this_thread::sleep(500);
        output.write_line("ct: resume 2");
        this_thread::sleep(100);
    });

    // start threads
    basic_parallel.invoke();
    shared_data.invoke();
    channel_data.invoke();
    coroutine.invoke();

    // shared data
    mut.lock();
    output.write_line("mt: push " + this_thread::get_id());
    ids.push(this_thread::get_id());
    mut.unlock();

    // channel
    for (usize i = 0; i < 8; i++)
    {
        string value = crypto::hash_hex(digests::md5(), crypto::random_bytes(8));
        output.write_line("cdt: push " + value);
        channel_data.push(value);
    }
    channel_data.push(string());

    // coroutine
    output.write_line("mt: sleep 500");
    this_thread::sleep(1500);
    coroutine.resume();
    output.write_line("mt: sleep 500");
    this_thread::sleep(1500);
    coroutine.resume();

    // join
    basic_parallel.join();
    shared_data.join();
    channel_data.join();
    coroutine.join();
    return 0;
}