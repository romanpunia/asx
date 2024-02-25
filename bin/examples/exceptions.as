import from { "exception", "thread", "console" };

class thread_routine
{
    bool wants_crash = false;

    void main(thread@ self)
    {
        try
        {
            throw exception_ptr("emotional damage", "this thread is too basic to exist"); // will affect only this thread
        }
        catch
        {
            auto error = exception::unwrap(); // get caught exception
            if (error.type != "emotional damage" || wants_crash)
                exception::rethrow(); // will rethrow, this thread will die here
            
            console::get().write_line("oh no, " + error.what()); // what() has info about stack frame
        }
    }
}

void start(bool wants_crash)
{
    thread_routine routine;
    routine.wants_crash = wants_crash;

    thread@ basic = thread(thread_parallel(@routine.main));
    basic.invoke();
    
    this_thread::sleep(1000);
    console::get().write_line("O_O"); // We are still OK
    basic.join();
}
int main(string[]@ args)
{ 
    start(args.size() > 1 ? args[1] == "crash" : false);
    return 0;
}