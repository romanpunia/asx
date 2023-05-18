#include <std/console.as>
#pragma cimport("kernel32.dll", "GetCurrentProcessId", "uint32 win32_get_pid()")
#pragma cimport("kernel32.dll", "Sleep", "void win32_sleep(uint32)")
#pragma cimport("kernel32.dll", "Beep", "uint32 win32_beep(uint32, uint32)")
#ifdef SOF_Beep
#define Beep(hz, ms) win32_beep(hz, ms)
#define OutputIfBeepCompileTime(x) output.write_line(x)
#else
#define Beep(_, _) {}
#define OutputIfBeepCompileTime(_) {}
#endif
#ifdef SOF_Sleep
#define Sleep(ms) win32_sleep(ms)
#else
#define Sleep(_) {}
#endif
#ifdef SOF_GetCurrentProcessId
#define GetCurrentProcessId() win32_get_pid()
#else
#define GetCurrentProcessId() 0
#endif

console@ output = console::get();

void beep_sleep(uint32 frequency, uint32 duration, uint32 wait)
{
    if (duration > 0)
        Beep(frequency, duration);

    if (wait > 70)
        Sleep(wait - 70);
    
    output.write_line("beep: (" + to_string(frequency) + "hz, " + to_string(duration) + "ms, " + to_string(wait) + "ms)");
}
int main()
{
    output.write_line("process: " + to_string(GetCurrentProcessId()));
    OutputIfBeepCompileTime("playing: mario");
    beep_sleep(330, 150, 100);
    beep_sleep(330, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(330, 150, 300);
    beep_sleep(392, 150, 700);
    beep_sleep(196, 150, 700);
    beep_sleep(262, 300, 300);
    beep_sleep(196, 300, 300);
    beep_sleep(164, 300, 300);
    beep_sleep(220, 300, 100);
    beep_sleep(246, 150, 300);
    beep_sleep(233, 200, 0);
    beep_sleep(220, 150, 300);
    beep_sleep(196, 150, 150);
    beep_sleep(330, 150, 150);
    beep_sleep(392, 150, 150);
    beep_sleep(440, 150, 300);
    beep_sleep(349, 150, 100);
    beep_sleep(392, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 100);
    beep_sleep(247, 150, 500);
    beep_sleep(262, 300, 300);
    beep_sleep(196, 300, 300);
    beep_sleep(164, 300, 300);
    beep_sleep(220, 300, 100);
    beep_sleep(246, 150, 300);
    beep_sleep(233, 200, 0);
    beep_sleep(220, 150, 300);
    beep_sleep(196, 150, 150);
    beep_sleep(330, 150, 150);
    beep_sleep(392, 150, 150);
    beep_sleep(440, 150, 300);
    beep_sleep(349, 150, 100);
    beep_sleep(392, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 100);
    beep_sleep(247, 150, 900);
    beep_sleep(392, 150, 100);
    beep_sleep(370, 150, 100);
    beep_sleep(349, 150, 100);
    beep_sleep(311, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(207, 150, 100);
    beep_sleep(220, 150, 100);
    beep_sleep(262, 150, 300);
    beep_sleep(220, 150, 100);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 500);
    beep_sleep(392, 150, 100);
    beep_sleep(370, 150, 100);
    beep_sleep(349, 150, 100);
    beep_sleep(311, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(523, 150, 300);
    beep_sleep(523, 150, 100);
    beep_sleep(523, 150, 1100);
    beep_sleep(392, 150, 100);
    beep_sleep(370, 150, 100);
    beep_sleep(349, 150, 100);
    beep_sleep(311, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(207, 150, 100);
    beep_sleep(220, 150, 100);
    beep_sleep(262, 150, 300);
    beep_sleep(220, 150, 100);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 500);
    beep_sleep(311, 300, 300);
    beep_sleep(296, 300, 300);
    beep_sleep(262, 300, 1300);
    beep_sleep(262, 150, 100);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 300);
    beep_sleep(330, 200, 50);
    beep_sleep(262, 200, 50);
    beep_sleep(220, 200, 50);
    beep_sleep(196, 150, 700);
    beep_sleep(262, 150, 100);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 100);
    beep_sleep(330, 150, 700);
    beep_sleep(440, 150, 300);
    beep_sleep(392, 150, 500);
    beep_sleep(262, 150, 100);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(294, 150, 300);
    beep_sleep(330, 200, 50);
    beep_sleep(262, 200, 50);
    beep_sleep(220, 200, 50);
    beep_sleep(196, 150, 700);
    beep_sleep(330, 150, 100);
    beep_sleep(330, 150, 300);
    beep_sleep(330, 150, 300);
    beep_sleep(262, 150, 100);
    beep_sleep(330, 150, 300);
    beep_sleep(392, 150, 700);
    beep_sleep(196, 150, 700);
    beep_sleep(196, 150, 125);
    beep_sleep(262, 150, 125);
    beep_sleep(330, 150, 125);
    beep_sleep(392, 150, 125);
    beep_sleep(523, 150, 125);
    beep_sleep(660, 150, 125);
    beep_sleep(784, 150, 575);
    beep_sleep(660, 150, 575);
    beep_sleep(207, 150, 125);
    beep_sleep(262, 150, 125);
    beep_sleep(311, 150, 125);
    beep_sleep(415, 150, 125);
    beep_sleep(523, 150, 125);
    beep_sleep(622, 150, 125);
    beep_sleep(830, 150, 575);
    beep_sleep(622, 150, 575);
    beep_sleep(233, 150, 125);
    beep_sleep(294, 150, 125);
    beep_sleep(349, 150, 125);
    beep_sleep(466, 150, 125);
    beep_sleep(587, 150, 125);
    beep_sleep(698, 150, 125);
    beep_sleep(932, 150, 575);
    beep_sleep(932, 150, 125);
    beep_sleep(932, 150, 125);
    beep_sleep(932, 150, 125);
    beep_sleep(1046, 675, 0);
    output.write_line("process: complete");
    output.read(4);

    return 0;
}