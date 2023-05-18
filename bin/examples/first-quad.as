#include <std/timestamp.as>
#include <std/activity.as>
#include <std/graphics.as>
#include <std/math.as>
#include <std/console.as>

activity@ window = null;
graphics_device@ device = null;
bool application_active = true;

void render_vertex(const vector2&in pos, const vector2&in tx, const vector3&in color)
{
    device.im_emit();
    device.im_position(pos.x, pos.y, 0);
    device.im_texcoord(tx.x, tx.y);
    device.im_color(color.x, color.y, color.z, 1);
}
void render_pass(uint64 start_time)
{
    render_target_2d@ target = device.get_render_target();
    float w = target.get_width(), h = target.get_height();
    float t = float(timestamp().milliseconds() - start_time);
    float x = sin(t), y = cos(t), z = sin(t / 2.0f);

    device.set_target();
    device.clear(0.0f, 0.0f, 0.0f);
    device.clear_depth();
    device.im_begin();
    {
        device.im_transform(
            matrix4x4::create_rotation_z(angle_saturate(deg2rad() * t * 0.05f)) *
            matrix4x4::create_translated_scale(vector3(w / 2.0f, h / 2.0f), vector3(h / 8.0f, h / 8.0f)) *
            matrix4x4::create_orthographic_off_center(0, w, h, 0.0f, -100.0f, 100.0f));

        render_vertex(vector2(-0.5f, 0.5f),  vector2(0.0f, 0.0f),  vector3(0.0f, 1.0f, 1.0f));
        render_vertex(vector2(-0.5f, -0.5f), vector2(0.0f, -1.0f), vector3(1.0f, 0.0f, 1.0f));
        render_vertex(vector2(0.5f, -0.5f),  vector2(1.0f, -1.0f), vector3(1.0f, 1.0f, 0.0f));
        render_vertex(vector2(0.5f, 0.5f),   vector2(1.0f, 0.0f),  vector3(1.0f, 0.0f, 1.0f));
        render_vertex(vector2(-0.5f, 0.5f),  vector2(0.0f, 0.0f),  vector3(0.0f, 1.0f, 1.0f));
        render_vertex(vector2(0.5f, -0.5f),  vector2(1.0f, -1.0f), vector3(1.0f, 1.0f, 0.0f));
    }
    device.im_end();
    device.submit();
}
int main()
{
    activity_desc window_desc;
    window_desc.width = 800;
    window_desc.height = 600;
    window_desc.gpu_as_renderer = true;
    window_desc.render_even_if_inactive = true;

    @window = activity(window_desc);
    window.set_window_state_change(function(window_state state, int x, int y)
    {
        if (state == window_state::resize)
            device.resize_buffers(x, y);
        else if (state == window_state::close)
            application_active = false;
    });
    
    graphics_device_desc device_desc;
    @device_desc.window = window;
    device_desc.vsync_mode = vsync::off;
    device_desc.buffer_width = window_desc.width;
    device_desc.buffer_height = window_desc.height;

    @device = graphics_device::create(device_desc);  
    window.show();
    window.maximize();

    uint64 start_time = timestamp().milliseconds();
    while (application_active)
    {
        window.dispatch();
        render_pass(start_time);
    }

    return 0;
}