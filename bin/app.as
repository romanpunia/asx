#include "std/console"
#include "std/schedule"
#include "std/random"
#include "std/vectors"
#include "std/thread"

class image_point
{
    vector2 point;
    uint8 character;
    float speed;

    image_point()
    {
    }
    image_point(uint32 width, uint32 height)
    {
        speed = random::getf();
        point = vector2::random_abs();
        point.x *= float(width);
        point.y = -point.y * float(height);
        randomize_char();
    }
    void randomize_char()
    {
        character = uint8(random::betweeni(32, 72));
    }
}

class image_fill
{
    image_point[] points;
    uint32 x, y, size;
    string image;

    image_fill()
    {
        resize();
    }
    void resize()
    {
        console@ output = console::get();
        uint32 new_x, new_y;
        output.get_size(new_x, new_y);

        if (new_x == x && new_y == y)
            return;

        x = new_x;
        y = new_y;
        size = x * y;

        image.resize(size);
        for (uint32 i = 0; i < size; i++)
            image[i] = uint8(random::betweeni(32, 72));

        points.resize(x);
        for (usize i = 0; i < points.size(); i++)
            points[i] = image_point(x, y);
    }
    void flush()
    {
        console@ output = console::get();
        output.clear();
        output.set_cursor(0, 0);
        output.write(image);
        output.flush_write();
    }
    void loop()
    {
        uint8 empty = ' ';
        for (uint32 i = 0; i < size; i++)
        {
            uint8 color = image[i];
            if (color < empty)
                ++image[i];
            else if (color > empty)
                --image[i];
        }

        for (usize i = 0; i < points.size(); i++)
        {
            image_point where = points[i];
            points[i].point.y += where.speed;

            int32 height = int32(where.point.y);
            if (height >= int32(y))
            {
                points[i] = image_point(x, y);
                continue;
            }
            else if (height < 0)
                continue;
            else if (height != int32(points[i].point.y))
                points[i].randomize_char();

            uint32 index = uint32(where.point.x) + uint32(height) * x;
            image[index] = where.character;
        }

        flush();
        resize();
    }
}

int main()
{
    schedule_policy policy;
    policy.set_threads(4);

    schedule@ queue = schedule::get();
    queue.start(policy);

    image_fill main;
    queue.set_interval(50, task_event(main.loop));
    
    return 0;
}