#include "std/engine"
#include "std/components"
#include "std/renderers"
#include "std/random"
#include "std/thread"
#include "std/console"

class runtime
{
    vector3 grid_size = 5.0f;
    float grid_radius = 15.0f;
    usize grid_materials = 32; 
    application@ self;

    runtime(application_desc&in init, const vector3&in size, float radius)
    {
        if (size.length() > 0.0f)
            grid_size = size;
        
        if (radius > 0.0f)
            grid_radius = radius;
        
        @self = application(init);
        @self.initialize = initialize_callback(this.initialize);
        @self.dispatch = dispatch_callback(this.dispatch);
        @self.publish = publish_callback(this.publish);
        @self.window_event = window_event_callback(this.window_event);
    }
    void initialize()
    {
        model@ cube = cast<model@>(self.content.load(self.content.get_processor(component_id("model")), "cube.obj"));
        if (cube is null)
        {
            self.stop();
            return;
        }

        @self.scene = scene_graph(scene_graph_desc::get(self));

        scene_entity@ camera = self.scene.get_camera_entity();
        camera.add_component(free_look_component(camera));
        camera.add_component(fly_component(camera));

        render_system@ system = self.scene.get_renderer();
        system.add_renderer(model_renderer(system));

        for (usize i = 0; i < grid_materials; i++)
        {
            material@ next = self.scene.add_material();
            next.surface.diffuse = vector3::random_abs();
        }

        for (float x = -grid_size.x; x < grid_size.x; x++)
        {
            for (float y = -grid_size.y; y < grid_size.y; y++)
            {
                for (float z = -grid_size.z; z < grid_size.z; z++)
                {
                    scene_entity@ next = self.scene.add_entity();
                    next.get_transform().set_position(vector3(x, y, z) * grid_radius);
                    
                    model_component@ drawable = cast<model_component@>(next.add_component(model_component(next)));
                    drawable.set_drawable(cube);
                    drawable.set_material(self.scene.get_material(random::betweeni(0, grid_materials)));
                }
            }
        }
    }
    void dispatch(clock_timer@ time)
    {
        self.scene.dispatch(time);
    }
    void publish(clock_timer@ time)
    {
        self.renderer.clear(0, 0, 0);
        self.renderer.clear_depth();
        self.scene.publish(time);
        self.scene.submit();
        self.renderer.submit();
    }
    void window_event(window_state state, int x, int y)
    {
        switch (state)
        {
            case window_state::resize:
                self.renderer.resize_buffers(x, y);
                if (self.scene !is null)
                    self.scene.resize_buffers();
                break;
            case window_state::close:
                self.stop();
                break;
        }
    }
}

int main(string[]@ args)
{
    application_desc init;
    init.graphics.vsync_mode = vsync::off;
    init.window.maximized = true;
    init.environment = "content";

    console@ output = console::get();
    output.show();
    
    output.write_line("type in grid size (default 5):");
    string ssize = output.read(16); float size = 5.0f;
    if (!ssize.empty())
    {
        float value = to_float(ssize);
        if (value > 0.0f)
            size = value;
    }

    output.write_line("type in grid radius (default 15):");
    string sradius = output.read(16); float radius = 15.0f;
    if (!sradius.empty())
    {
        float value = to_float(sradius);
        if (value > 0.0f)
            radius = value;
    }

    float entities = size * size * size * 8;
    if (entities > 64000.0f)
        output.write_line("0. this man is insane");

    output.write_line("1. will spawn " + to_string(entities) + " entities");
    output.write_line("2. will separate entities by " + to_string(radius) + " units distance");
    output.write_line("3. will begin in 3 seconds");
    this_thread::sleep(3000);

    runtime app(init, size, radius);
    return app.self.start();
}