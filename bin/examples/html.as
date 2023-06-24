#include <std/engine.as>

class runtime
{
    application@ self;
    gui_context@ context;

    runtime(application_desc&in init)
    {
        @self = application(init);
        self.set_on_initialize(initialize_callback(this.initialize));
        self.set_on_dispatch(dispatch_callback(this.dispatch));
        self.set_on_publish(publish_callback(this.publish));
        self.set_on_window_event(window_event_callback(this.window_event));
        self.set_on_fetch_gui(fetch_gui_callback(this.get_gui));
    }
    void initialize()
    {
        @context = gui_context(@self.renderer);
        if (!context.initialize("manifest.xml"))
            self.stop();
    }
    void dispatch(clock_timer@ time)
    {
        if (!self.has_processed_events())
            self.window.dispatch_blocking(context.get_idle_timeout_ms(10000));
        context.update_events(@self.window);
    }
    void publish(clock_timer@ time)
    {
        self.renderer.clear(0, 0, 0);
        self.renderer.clear_depth();
        context.render_lists(null);
        self.renderer.submit();
    }
    void window_event(window_state state, int x, int y)
    {
        switch (state)
        {
            case window_state::resize:
                self.renderer.resize_buffers(x, y);
                break;
            case window_state::close:
                self.stop();
                break;
        }
    }
    gui_context@ get_gui()
    {
        return @context;
    }
}

int main()
{
    application_desc init;
    init.window.maximized = true;
    init.environment = "assets";

    runtime app(init);
    return app.self.start();
}