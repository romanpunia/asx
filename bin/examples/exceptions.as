#include <std/exception.as>
#include <std/thread.as>
#include <std/console.as>

class thread_routine
{
    /*
        you may also want to try -j option
        that will activate the JIT compiler,
        you will see how much less information
        you get because most of the code is
        being translated to machine instructions.
    */
    bool wants_crash = false;

    void main(thread@ self)
    {
        try
        {
            throw exception_ptr("emotional damage", "this thread is too basic to exist"); // will affect only this thread
        }
        catch
        {
            auto error = exception::unwrap(); // get catched exception
            if (error.type != "emotional damage" || wants_crash)
                exception::rethrow(); // will rethrow and propagate to joiner, this thread will die here
            
            console::get().write_line("oh no, " + error.what()); // what() has info about stack frame
        }
    }
}

void start(bool wants_crash)
{
    thread_routine routine;
    routine.wants_crash = wants_crash;

    thread@ basic = thread(thread_event(@routine.main));
    basic.invoke();
    
    this_thread::sleep(1000);
    console::get().write_line("O_O"); // We are still OK
    basic.join(); // crash might happen here if joinable thread will rethrow the exception
}
int main(string[]@ args)
{ 
    start(args.size() > 1 ? args[1] == "crash" : false);
    return 0;
}