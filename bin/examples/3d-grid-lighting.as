import from
{
    "engine",
    "engine-components",
    "engine-renderers",
    "random",
    "thread",
    "console",
    "math"
};

class runtime
{
    vector3 grid_size = 5.0f;
    float grid_radius = 15.0f;
    usize grid_materials = 32; 
    float timing = 0.0;
    heavy_application@ self;

    runtime(heavy_application_desc&in init, const vector3&in size, float radius)
    {
        grid_size = size.length() > 0.0f ? size : 5.0f;
        grid_radius = radius > 0.0f ? radius : 15.0f;
        @self = heavy_application(init, @this);
        self.set_on_initialize(initialize_sync(this.initialize));
        self.set_on_dispatch(dispatch_sync(this.dispatch));
        self.set_on_publish(publish_sync(this.publish));
        self.set_on_window_event(window_event_sync(this.window_event));
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
        fly.moving.faster = 700.0f;
        camera.add_component(fly);
        camera.add_component(free_look_component(camera));
        
        auto@ viewer = cast<camera_component@>(camera.get_component(component_id("camera_component")));
        viewer.far_plane = 600;
        
        render_system@ system = self.scene.get_renderer();
        system.add_renderer(model_renderer(system));
        system.add_renderer(lighting_renderer(system));
        
        /* Screen-space ray tracing */
        local_illumination_renderer@ local_illumination = local_illumination_renderer(system);
        local_illumination.indirection.distance = grid_radius;
        local_illumination.indirection.swing = 0.4;
        system.add_renderer(@local_illumination);

        scene_entity@ light = self.scene.add_entity();
        {
            transform@ where = light.get_transform();
            where.set_position(vector3(-10.0f, 10.0f, -10.0f));

            line_light_component@ line = cast<line_light_component@>(light.add_component(line_light_component(light)));
            line.shadow.distance0 = grid_size.x * grid_radius * 1.5f;
            line.shadow.distance1 = grid_size.x * grid_radius * 2.5f;
            line.shadow.cascades = 2;
            line.shadow.enabled = true;
            line.shadow.softness = 10.0f;
            line.shadow.iterations = 64;
            line.shadow.bias = 0.00001f;
            line.emission = 4.0f;
        }
        light.set_name("light");

        for (usize i = 0; i < grid_materials; i++)
        {
            material@ next = self.scene.add_material();
            next.surface.diffuse = vector3::random_abs();
            next.surface.roughness.x = 0.3 * randomf();
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
        
        auto@ light = cast<line_light_component@>(self.scene.get_component(component_id("line_light_component"), 0));
        if (self.window.is_key_down_hit(key_code::g))
            light.shadow.enabled = !light.shadow.enabled;

        float delta = time.get_elapsed() * 0.1;
        float x = sinf(delta) * 2.0, y = cosf(delta) * 2.0;
        light.get_entity().get_transform().set_position(vector3(-10.0f * x, 10.0f, -10.0f * y));

        const float elapsed = time.get_elapsed_mills();
        if (elapsed - timing > 500)
        {
            const usize batching = self.scene.statistics.batching;
            const usize sorting = self.scene.statistics.sorting;
            const usize draw_calls = self.scene.statistics.draw_calls;
            const usize instances = self.scene.statistics.instances;
            const usize entities = self.scene.get_entities_count() - 2;
            const string shadows = light.shadow.enabled ? "on" : "off";
            const string title =
                "draw_calls = " + to_string(draw_calls) + ", " +
                "instances = " + to_string(instances) + " (" + to_string(100 * instances / double(entities)) + "%), " +
                "fps = " + to_string(uint64(time.get_frames())) + ", " +
                "sorting = " + to_string(sorting) + ", " +
                "batching = " + to_string(batching) + ", " +
                "shadows = " + shadows;
            self.window.set_title(title);
            timing = elapsed;
        }

        self.scene.dispatch(time);
    }
    void publish(clock_timer@ time)
    {
        self.scene.publish_and_submit(time, 0, 0, 0, false);
    }
    void window_event(window_state state, int x, int y)
    {
        switch (state)
        {
            case window_state::resize:
                self.renderer.rescale_buffers(x, y);
                if (self.scene !is null)
                    self.scene.resize_buffers();
                break;
            case window_state::close:
                self.stop();
                break;
        }
    }
}

[#console::main]
int main()
{
    heavy_application_desc init;
    init.graphics.vsync_mode = vsync::off;
    init.window.maximized = true;
    init.environment = "assets";

    console@ output = console::get();
    output.write("type in grid size (default 5): ");
    float size = to_float(output.read(16));
    if (size <= 0.0f)
        size = 5.0f;

    output.write("type in grid radius (default 4): ");
    float radius = to_float(output.read(16));
    if (radius <= 0.0f)
        radius = 4.0f;

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