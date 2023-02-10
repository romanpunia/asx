#include "std/timestamp"
#include "std/activity"
#include "std/graphics"
#include "std/math"
#include "std/console"

activity@ window = null;
graphics_device@ device = null;
bool application_active = true;

void render_pass(uint64 start_time)
{
    render_target_2d@ target = device.get_render_target();
    float w = target.get_width(), h = target.get_height();
    float m = (device.is_left_handed() ? 1.0f : -1.0f);
    float t = float(timestamp().milliseconds() - start_time) * 0.001f;
    float x = sin(t), y = cos(t), z = sin(t / 2.0f);

    device.set_target();
    device.clear(0.0f, 0.0f, 0.0f);
    device.clear_depth();
    {
        device.im_begin();
        device.im_transform(
            matrix4x4::create_translation(vector3(x, y, 3.0)) *
            matrix4x4::create_scale(0.5) *
            matrix4x4::create_rotation_z(z) *
            matrix4x4::create_perspective(60.0, w / h, 0.1, 100.0));

        device.im_emit();
        device.im_position(-0.5, -0.5f * m, 0);
        device.im_texcoord(0.0, 0.0);
        device.im_color(0, 1, 1, 1);

        device.im_emit();
        device.im_position(-0.5, 0.5f * m, 0);
        device.im_texcoord(0.0, 1.0);
        device.im_color(1, 0, 1, 1);

        device.im_emit();
        device.im_position(0.5, 0.5f * m, 0);
        device.im_texcoord(1.0, 1.0);
        device.im_color(1, 1, 0, 1);

        device.im_emit();
        device.im_position(0.5, -0.5f * m, 0);
        device.im_texcoord(1.0, 0.0);
        device.im_color(1, 1, 0, 1);

        device.im_emit();
        device.im_position(-0.5, -0.5f * m, 0);
        device.im_texcoord(0.0, 0.0);
        device.im_color(0, 1, 1, 1);

        device.im_emit();
        device.im_position(0.5, 0.5f * m, 0);
        device.im_texcoord(1.0, 1.0);
        device.im_color(1, 1, 0, 1);
        device.im_end();
    }
    device.submit();
}
int main(string[]@ args)
{
    activity_desc window_desc;
    window_desc.width = 800;
    window_desc.height = 600;
    window_desc.allow_graphics = true;
    window_desc.allow_stalls = false;

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