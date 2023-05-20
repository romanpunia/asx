#include <std/engine.as>
#include <std/components.as>
#include <std/renderers.as>
#include <std/math.as>

/*
    Warning: this example wants active internet connection,
    otherwise no diffuse texture will be set
*/
class runtime
{
    application@ self;

    runtime(application_desc&in init)
    {
        @self = application(init);
        @self.initialize = initialize_callback(this.initialize);
        @self.dispatch = dispatch_callback(this.dispatch);
        @self.publish = publish_callback(this.publish);
        @self.window_event = window_event_callback(this.window_event);
    }
    void initialize()
    {
        texture_2d@ diffuse = cast<texture_2d@>(self.content.load(self.content.get_processor(component_id("texture_2d")), "https://img.freepik.com/free-vector/simple-pattern-background_1319-147.jpg"));
        texture_2d@ normal = cast<texture_2d@>(self.content.load(self.content.get_processor(component_id("texture_2d")), "normal.jpg"));
        model@ cube = cast<model@>(self.content.load(self.content.get_processor(component_id("model")), "cube.obj"));
        if (cube is null)
        {
            self.stop();
            return;
        }

        scene_graph_desc desc = scene_graph_desc::get(self);
        desc.simulator.gravity.y = -3.63f;
        desc.lines_size = 2048;

        @self.scene = scene_graph(desc);
        physics_simulator@ physics = self.scene.get_simulator();
        
        material@ floor_material = self.scene.add_material();
        floor_material.surface.diffuse = 0.75f;
        floor_material.surface.roughness.x = 0.445f;
        floor_material.set_diffuse_map(diffuse);
        floor_material.set_normal_map(normal);
        floor_material.set_name("floor-material");
        
        material@ top_material = self.scene.add_material();
        top_material.surface.roughness.x = 0.445f;
        top_material.set_diffuse_map(diffuse);
        top_material.set_name("top-material");

        scene_entity@ camera = self.scene.get_camera_entity();
        {
            transform@ where = camera.get_transform();
            where.set_rotation(vector3(deg2rad() * 30.0f, 0.0f, 0.0f));
            where.set_position(vector3(0.0f, 5.0f, -10.0f));
            
            camera.add_component(free_look_component(camera));
            camera.add_component(fly_component(camera));
        }
        camera.set_name("camera");

        render_system@ system = self.scene.get_renderer();
        system.add_renderer(model_renderer(system));
        system.add_renderer(lighting_renderer(system));
        
        scene_entity@ light = self.scene.add_entity();
        {
            transform@ where = light.get_transform();
            where.set_position(vector3(-10.0f, 10.0f, -10.0f));

            line_light_component@ line = cast<line_light_component@>(light.add_component(line_light_component(light)));
            line.shadow.enabled = true;
            line.shadow.softness = 1.0f;
            line.shadow.iterations = 64;
            line.shadow.bias = 0.00005f;
            line.sky.rlh_height = 1000.0f;
            line.sky.mie_height = 1000.0f;
            line.sky.intensity = 12.0f;
            line.emission = 4.0f;
        }
        light.set_name("light");

        scene_entity@ floor = self.scene.add_entity();
        {
            transform@ where = floor.get_transform();
            where.set_position(vector3(0.0f, -2.0f, 0.0f));
            where.set_scale(vector3(50.0f, 1.0f, 50.0f));
            
            model_component@ drawable = cast<model_component@>(floor.add_component(model_component(floor)));
            drawable.texcoord = vector2(20.0f, 20.0f);
            drawable.set_drawable(cube);
            drawable.set_material(floor_material);
            
            rigid_body_component@ body = cast<rigid_body_component@>(floor.add_component(rigid_body_component(floor)));
            body.load(physics.create_cube(), 0.0f);

            physics_rigidbody@ main = body.get_body();
            main.set_spinning_friction(0.9);
            main.set_friction(1.2);
            main.set_restitution(0.3);
        }
        floor.set_name("floor");

        vector3 grid_size = 2.0f;
        float grid_radius = 3.0f;
        usize grid_index = 0;
        
        for (float x = -grid_size.x; x < grid_size.x; x++)
        {
            for (float y = 0.0f; y < grid_size.y * 2.0f; y++)
            {
                for (float z = -grid_size.z; z < grid_size.z; z++)
                {
                    scene_entity@ top = self.scene.add_entity();
                    {
                        transform@ where = top.get_transform();
                        where.set_position(vector3(x, y, z) * grid_radius);

                        model_component@ drawable = cast<model_component@>(top.add_component(model_component(top)));
                        drawable.set_drawable(cube);
                        drawable.set_material(top_material);

                        rigid_body_component@ body = cast<rigid_body_component@>(top.add_component(rigid_body_component(top)));
                        body.load(physics.create_cube(), 10.0f);
                                    
                        physics_rigidbody@ main = body.get_body();
                        main.set_spinning_friction(0.9);
                        main.set_friction(1.2);
                        main.set_restitution(0.3);
                    }
                    top.set_name("top[" + to_string(grid_index++) + "]");
                }
            }
        }
    }
    void dispatch(clock_timer@ time)
    {
        if (self.window.is_key_down_hit(key_code::cursor_left))
        {
            auto@ entities = self.scene.query_by_name("top[0]");
            if (!entities.empty())
            {
                scene_entity@ copy = self.scene.clone_entity(entities[0]);
                scene_entity@ camera = self.scene.get_camera_entity();
                ray where = cast<camera_component@>(camera.get_component(component_id("camera_component"))).get_cursor_ray();
                copy.get_transform().set_position(where.origin);
                copy.set_name("top-copy");
                
                auto@ body = cast<rigid_body_component@>(copy.get_component(component_id("rigid_body_component")));
                if (body !is null)
                {
                    auto@ main = body.get_body();
                    main.set_linear_velocity(where.direction * 20.0f);
                    main.set_angular_velocity(vector3::random());
                    body.set_transform(true);
                }
            }
        }
        else if (self.window.is_key_down_hit(key_code::cursor_middle))
        {
            auto@ entities = self.scene.query_by_name("light");
            if (!entities.empty())
            {
                scene_entity@ camera = self.scene.get_camera_entity();
                transform@ where = camera.get_transform();
                entities[0].get_transform().set_position(where.get_position());
            }
        }

        if (self.window.is_key_down_hit(key_code::t))
        {
            physics_simulator@ physics = self.scene.get_simulator();
            physics.create_linear_impulse(vector3::random() * 30.0, true);
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
    init.graphics.vsync_mode = vsync::on;
    init.window.maximized = true;
    init.environment = "content";

    runtime app(init);
    return app.self.start();
}