/*
    This is a simple test that will load a all cores
    by computing simple integer hash in a loop within
    multiple workers. This example is an illustration
    of multicore CPU utilisation. Also it can shows
    how to use native multithreading within VM.
*/
import from { "console", "os", "thread" };

class test_worker
{
    int32[]@ hashes = null;
    int32 index = 0;
    int32 value = 0;
    usize group = 0;

    void execute(thread@)
    {
        int32 hash = index, max = 2 << 29;
        while (value > 0)
            hash = ((hash << 5) - hash + value--) % max;
        hashes[group] = hash;
    }
}

int32[]@ test(int32 value)
{
    usize threads_count = usize(os::cpu::get_quantity_info().logical);
    thread@[] threads = array<thread@>();
    int32[]@ hashes = array<int32>();
    threads.reserve(threads_count);
    hashes.reserve(threads_count);

    /* prepare shared array beforehand to avoid synchronizations */
    for (usize i = 0; i < threads_count; i++)
        hashes.push(i * 4);
    
    /* spawn threads and their thread-local data */
    for (usize i = 0; i < threads_count; i++)
    {
        test_worker@ worker = test_worker();
        worker.index = hashes[i];
        worker.value = value;
        worker.group = i;

        /*
            Because each thread only writes to
            it's own location of this shared array,
            we don't need any synchronization
        */
        @worker.hashes = hashes;

        thread@ next = thread(thread_event(worker.execute));
        next.invoke();
        threads.push(@next);
    }

    /* wait for all threads to finish */
    for (usize i = 0; i < threads_count; i++)
        threads[i].join();

    return hashes;
}
int main(string[]@ args)
{
    auto@ term = console::get();
    term.capture_time();
    if (args.empty())
    {
        term.write_line("provide test sequence index");
        term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
        return 1;
    }

    int32 index = to_int32(args[args.size() - 1]);
    if (index <= 0)
    {
        term.write_line("invalid test sequence index");
        term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
        return 2;
    }

    int32[]@ hashes = test(index);
    for (usize i = 0; i < hashes.size(); i++)
    {
        string value = to_string(hashes[i]);
        term.write_line("worker result #" + to_string(i + 1) + ": " + value);
    }

    term.write_line("time: " + to_string(term.get_captured_time()) + "ms");
    return 0;
}