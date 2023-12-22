import from "engine";

/*
    We will use standard entity sharing
    system that exists in AngelScript to
    expose this class to our HTML application
    scripts.
 */
shared class runtime
{
    application@ self;
    gui_context@ context;

    runtime(application_desc&in init)
    {
        @self = application(init, @this);
        self.set_on_initialize(initialize_callback(this.initialize));
        self.set_on_dispatch(dispatch_callback(this.dispatch));
        self.set_on_publish(publish_callback(this.publish));
        self.set_on_window_event(window_event_callback(this.window_event));
        self.set_on_fetch_gui(fetch_gui_callback(this.get_gui));
    }
    void initialize()
    {
        @context = gui_context(@self.renderer);
        context.load_font_face("sf-ui-display-regular.ttf");
        context.load_font_face("sf-ui-display-bold.ttf");
        
        ui_document document = context.load_document("application.html");
        if (document.is_valid())
            document.show();
        else
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

/*
    Then we expose a very specific function
    that will fetch currently running application
    and retrieve an initiator instance from it
    which we will cast to our target class
*/
shared runtime@ get_app()
{
    application@ self = application::get();
    if (self is null)
        return null;
    
    runtime@ app = null;
    if (!self.retrieve_initiator(@app))
        return null;
    
    return @app;
}

int main()
{
    application_desc init;
    init.window.maximized = true;
    init.environment = "assets";

    runtime app(init);
    return app.self.start();
}