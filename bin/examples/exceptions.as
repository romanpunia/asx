#include <std/exception.as>
#include <std/thread.as>

void start()
{
    thread@ basic = thread(function(thread@ self)
    {
        throw exception_ptr("test:exception");
    });

    int num = 0;
    basic.invoke();
    while (num < 10000)
        ++num;
    basic.join();
}
int main()
{ 
    start();
    return 0;
}