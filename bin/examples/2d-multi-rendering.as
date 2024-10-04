import from
{
    "timestamp",
    "activity",
    "graphics",
    "math",
    "random"
};

class render_window
{
    activity@ window = null;
    graphics_device@ device = null;
    vector3 saturation = vector3::random_abs();
    bool active = true;

    render_window(uint8 index, uint32 width, uint32 height)
    {
        activity_desc window_desc;
        window_desc.width = width;
        window_desc.height = height;
        window_desc.x = 40 + 20 * (index + 1);
        window_desc.y = window_desc.x;
        window_desc.title = "Render window #" + to_string(index + 1);
        window_desc.gpu_as_renderer = true;
        window_desc.render_even_if_inactive = true;

        display_info info;
        if (video::get_display_info(0, info))
        {
            window_desc.x = random::betweeni(40, info.width - window_desc.width);
            window_desc.y = random::betweeni(40, info.height - window_desc.height);
        }

        @window = activity(window_desc);
        window.set_window_state_change(window_state_change_sync(this.dispatch_events));
        
        graphics_device_desc device_desc;
        @device_desc.window = window;
        device_desc.vsync_mode = vsync::on;
        device_desc.buffer_width = window_desc.width;
        device_desc.buffer_height = window_desc.height;

        @device = graphics_device::create(device_desc);
        window.show();
    }
    void dispatch_events(window_state state, int x, int y)
    {
        if (state == window_state::close)
        {
            /* Cleanup a reference to this class instance */
            window.set_window_state_change(null);
            window.hide();
            active = false;
        }
        if (state == window_state::resize)
            device.resize_buffers(x, y);
    }
    void render_vertex(const vector2&in pos, const vector2&in tx, const vector3&in color)
    {
        device.im_emit();
        device.im_position(pos.x, pos.y, 0);
        device.im_texcoord(tx.x, tx.y);
        device.im_color(color.x, color.y, color.z, 1);
    }
    void render_pass(uint64 start_time)
    {
        float t = sinf(float(timestamp().milliseconds() - start_time) * 0.005f);
        vector2 cursor = window.get_global_cursor_position();
        float w = window.get_width(), h = window.get_height();
        float x = window.get_x(), y = window.get_y();

        device.set_target();
        device.clear(0.0f, 0.0f, 0.0f);
        device.clear_depth();
        device.im_begin();
        {
            device.im_transform(
                matrix4x4::create_translated_scale(vector3(cursor.x - x, cursor.y - y), vector3(80, 80)) *
                matrix4x4::create_orthographic_off_center(0, w, h, 0.0f, -100.0f, 100.0f));

            render_vertex(vector2(-0.5f, 0.5f),  vector2(0.0f, 0.0f),  vector3(0.0f, 1.0f, 1.0f).lerp(saturation, t));
            render_vertex(vector2(-0.5f, -0.5f), vector2(0.0f, -1.0f), vector3(1.0f, 0.0f, 1.0f).lerp(saturation, t));
            render_vertex(vector2(0.5f, -0.5f),  vector2(1.0f, -1.0f), vector3(1.0f, 1.0f, 0.0f).lerp(saturation, t));
            render_vertex(vector2(0.5f, 0.5f),   vector2(1.0f, 0.0f),  vector3(1.0f, 0.0f, 1.0f).lerp(saturation, t));
            render_vertex(vector2(-0.5f, 0.5f),  vector2(0.0f, 0.0f),  vector3(0.0f, 1.0f, 1.0f).lerp(saturation, t));
            render_vertex(vector2(0.5f, -0.5f),  vector2(1.0f, -1.0f), vector3(1.0f, 1.0f, 0.0f).lerp(saturation, t));
        }
        device.im_end();
        device.submit();
    }
}

int main()
{
    render_window@[] windows;
    for (uint8 i = 0; i < 8; i++)
        windows.push(render_window(i, 400, 400));

    activity_event_consumers events;
    for (usize i = 0; i < windows.size(); i++)
        events.push(@windows[i].window);

    uint64 start_time = timestamp().milliseconds();
    while (!windows.empty())
    {
        usize active = windows.size();
        activity::multi_dispatch(events);
        for (usize i = 0; i < windows.size(); i++)
        {
            render_window@ next = windows[i];
            if (!next.active)
            {
                events.pop(@next.window);
                if (i < windows.size())
                    windows.erase(i--);
            }
            else
                next.render_pass(start_time);
        }
    }

    return 0;
}