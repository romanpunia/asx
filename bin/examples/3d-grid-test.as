#include <std/engine.as>
#include <std/components.as>
#include <std/renderers.as>
#include <std/random.as>
#include <std/thread.as>
#include <std/console.as>

class runtime
{
    vector3 grid_size = 5.0f;
    float grid_radius = 15.0f;
    usize grid_materials = 32; 
    float timing = 0.0;
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

        usize total_entities = usize(grid_size.x * grid_size.y * grid_size.z) * 8;
        @self.scene = scene_graph(scene_graph_desc::get(self));
        self.scene.reserve_materials(grid_materials);
        self.scene.reserve_entities(total_entities);
        self.scene.reserve_components(component_id("model_component"), total_entities);

        scene_entity@ camera = self.scene.get_camera_entity();
        auto@ fly = fly_component(camera);
        fly.moving.faster = 4000.0f;
        camera.add_component(fly);
        camera.add_component(free_look_component(camera));
        
        auto@ viewer = cast<camera_component@>(camera.get_component(component_id("camera_component")));
        viewer.far_plane = 800;
        
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
        const float elapsed = time.get_elapsed_mills();
        if (elapsed - timing > 500)
        {
            const usize batching = self.scene.statistics.batching;
            const usize sorting = self.scene.statistics.sorting;
            const usize draw_calls = self.scene.statistics.draw_calls;
            const usize instances = self.scene.statistics.instances;
            const usize entities = self.scene.get_entities_count() - 1;
            const string title =
                "draw_calls = " + to_string(draw_calls) + ", " +
                "instances = " + to_string(instances) + " (" + to_string(100 * instances / double(entities)) + "%), " +
                "fps = " + to_string(uint64(time.get_frames())) + ", " +
                "sorting = " + to_string(sorting) + ", " +
                "batching = " + to_string(batching);
            self.window.set_title(title);
            timing = elapsed;
        }

        camera_component@ camera = cast<camera_component@>(self.scene.get_camera());
        if (self.window.is_key_down_hit(key_code::q))
            camera.far_plane -= 100;
        else if (self.window.is_key_down_hit(key_code::e))
            camera.far_plane += 100;

        if (self.window.is_key_down_hit(key_code::cursor_left))
        {
            base_component@[]@ components = self.scene.query_by_ray(component_id("model_component"), camera.get_cursor_ray());
            if (!components.empty())
                components[0].get_entity().get_transform().set_rotation(vector3::random());
        }

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

int main()
{
    application_desc init;
    init.window.maximized = true;
    init.environment = "content";

    console@ output = console::get();
    output.show();
    
    output.write("type in grid size (default 5): ");
    float size = to_float(output.read(16));
    if (size <= 0.0f)
        size = 5.0f;

    output.write("type in grid radius (default 15): ");
    float radius = to_float(output.read(16));
    if (radius <= 0.0f)
        radius = 15.0f;

    float entities = size * size * size * 8;
    if (entities > 64000.0f)
        output.write_line("0. this man is insane");

    output.write_line("1. will spawn " + to_string(entities) + " entities");
    output.write_line("2. will separate entities by " + to_string(radius) + " units distance");
    output.write_line("3. will begin in 1 second");
    this_thread::sleep(1000);

    runtime app(init, size, radius);
    return app.self.start();
}