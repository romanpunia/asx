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
    heavy_application@ self;
    int64 clip_now = 0;
    usize clip_count = 0;

    runtime(heavy_application_desc&in init)
    {
        /*
            Application constructor requires a reference to
            unspecified context. This context can be, for example,
            this class. Null can also be passed. Application sets
            a singleton that can be use to retrieve this context
            by casting it to target class:
                see html.as as reference
        */
        @self = heavy_application(init, @this);
        self.set_on_initialize(initialize_sync(this.initialize));
        self.set_on_dispatch(dispatch_sync(this.dispatch));
        self.set_on_publish(publish_sync(this.publish));
        self.set_on_window_event(window_event_sync(this.window_event));
    }
    void initialize()
    {
        base_processor@ load_model = self.content.get_processor(component_id("skin_model"));
        base_processor@ load_texture = self.content.get_processor(component_id("texture_2d"));
        base_processor@ load_animation = self.content.get_processor(component_id("skin_animation"));
        @self.scene = scene_graph(scene_graph_desc::get(self));

        scene_entity@ camera = self.scene.get_camera_entity();
        camera.add_component(free_look_component(camera));

        fly_component@ fly = fly_component(camera);
        fly.moving.faster /= 4.0;
        fly.moving.normal /= 4.0;
        fly.moving.slower /= 4.0;
        camera.add_component(@fly);

        render_system@ system = self.scene.get_renderer();
        system.add_renderer(skin_renderer(system));

        scene_entity@ wolf = self.scene.add_entity();
        wolf.get_transform().set_rotation(vector3(270 * deg2radf(), 0, 0));
        
        material@ body_material = self.scene.add_material();
        material@ eyes_material = self.scene.add_material();
        material@ fur_material = self.scene.add_material();
        material@ teeth_material = self.scene.add_material();
        material@ claws_material = self.scene.add_material();
        body_material.set_diffuse_map(cast<texture_2d@>(self.content.load(load_texture, "body.jpg")));
        fur_material.set_diffuse_map(cast<texture_2d@>(self.content.load(load_texture, "fur.png")));
        eyes_material.set_diffuse_map(cast<texture_2d@>(self.content.load(load_texture, "eyes.jpg")));
        teeth_material.surface.diffuse = vector3(0.226f, 0.232f, 0.188f);
        claws_material.surface.diffuse = vector3(0.141f, 0.141f, 0.141f);

        skin_component@ drawable = cast<skin_component@>(wolf.add_component(skin_component(wolf)));
        drawable.set_drawable(cast<skin_model@>(self.content.load(load_model, "wolf.fbx")));
        drawable.set_material_for("Wolf1_Material__wolf_col_tga_0", body_material);
        drawable.set_material_for("Wolf2_fur__fella3_jpg_001_0.001", fur_material);
        drawable.set_material_for("Wolf3_eyes_0", eyes_material);
        drawable.set_material_for("Wolf3_teeth__nor2_tga_0", teeth_material);
        drawable.set_material_for("Wolf3_claws_0", claws_material);

        skin_animator_component@ animator = cast<skin_animator_component@>(wolf.add_component(skin_animator_component(wolf)));
        animator.set_animation(cast<skin_animation@>(self.content.load(load_animation, "wolf.fbx")));
        animator.state.looped = true;

        clip_count = animator.get_clips_count();
    }
    void dispatch(clock_timer@ time)
    {
        bool go_to_left = self.window.is_key_down_hit(key_code::q);
        bool go_to_right = self.window.is_key_down_hit(key_code::e);
        if (self.window.is_key_down_hit(key_code::f))
        {
            skin_animator_component@ animation = cast<skin_animator_component@>(self.scene.get_component(component_id("skin_animator_component"), 0));
            if (animation !is null)
            {
                if (!animation.state.is_playing())
                    animation.play(clip_now);
                else
                    animation.pause();
            }
        }
        else if (go_to_left || go_to_right)
        {
            if (go_to_left && --clip_now < 0)
                clip_now = int64(clip_count) - 1;
            else if (go_to_right && ++clip_now > int64(clip_count))
                clip_now = 0;
            
            skin_animator_component@ animation = cast<skin_animator_component@>(self.scene.get_component(component_id("skin_animator_component"), 0));
            if (animation !is null)
                animation.play(clip_now);
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

int main()
{
    heavy_application_desc init;
    init.graphics.vsync_mode = vsync::on;
    init.window.maximized = true;
    init.environment = "assets";

    runtime app(init);
    return app.self.start();
}