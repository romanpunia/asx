import from "engine";

/*
    We will use standard entity sharing
    system that exists in AngelScript to
    expose this class to our HTML application
    scripts.
 */
shared class runtime
{
    heavy_application@ self;

    runtime(heavy_application_desc&in init)
    {
        @self = heavy_application(init, @this);
        self.set_on_initialize(initialize_sync(this.initialize));
        self.set_on_dispatch(dispatch_sync(this.dispatch));
        self.set_on_publish(publish_sync(this.publish));
        self.set_on_window_event(window_event_sync(this.window_event));
    }
    void initialize()
    {
        ui_context@ ui = self.fetch_ui();
        ui_document document = ui.load_document("application.html", true);
        if (document.is_valid())
            document.show();
        else
            self.stop();
    }
    void dispatch(clock_timer@ time)
    {
        ui_context@ ui = self.try_get_ui();
        if (!self.has_processed_events())
            self.window.dispatch(ui.get_idle_timeout_ms(10000));
        ui.update_events(@self.window);
    }
    void publish(clock_timer@ time)
    {
        ui_context@ ui = self.try_get_ui();
        self.renderer.clear(0, 0, 0);
        self.renderer.clear_depth();
        ui.render_lists(null);
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
}

/*
    Then we expose a very specific function
    that will fetch currently running application
    and retrieve an initiator instance from it
    which we will cast to our target class
*/
shared runtime@ get_app()
{
    heavy_application@ self = heavy_application::get();
    if (self is null)
        return null;
    
    runtime@ app = null;
    if (!self.retrieve_initiator(@app))
        return null;
    
    return @app;
}

int main()
{
    heavy_application_desc init;
    init.window.maximized = true;
    init.environment = "assets";

    runtime app(init);
    return app.self.start();
}