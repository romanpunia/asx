#include <std/console.as>
#include <std/schedule.as>
#include <std/random.as>
#include <std/vectors.as>
#include <std/math.as>

class image_point
{
    vector2 point;
    uint8 character;
    float speed;

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
    image_point@[] points;
    float[] seed;
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
        size = x * y - 1;

        image.resize(size);
        for (uint32 i = 0; i < size; i++)
            image[i] = uint8(random::betweeni(32, 72));
        image[size - 1] = ' ';

        seed.resize(size);
        for (uint32 i = 0; i < size; i++)
            seed[i] = random::getf();

        points.resize(x);
        for (usize i = 0; i < points.size(); i++)
            @points[i] = image_point(x, y);
    }
    void flush()
    {
        console@ output = console::get();
        output.set_cursor(0, 0);
        output.write(image);
        output.flush_write();
    }
    void loop_matrix()
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
            image_point@ where = points[i];
            where.point.y += where.speed;

            int32 height = int32(where.point.y);
            if (height >= int32(y))
            {
                @points[i] = image_point(x, y);
                continue;
            }
            else if (height < 0)
                continue;
                
            uint32 index = uint32(where.point.x) + uint32(height) * x;
            if (index < image.size())
                image[index] = where.character;
            where.randomize_char();
        }

        flush();
        resize();
    }
    void loop_noise()
    {
        for (uint32 i = 0; i < size; i++)
            image[i] = uint8(random::betweeni(32, 72));

        flush();
        resize();
    }
    void loop_perlin_1d()
    {
        int octaves = 9;
        for (int i = 0; i < int(size); i++)
        {
            float noise = 0.0, scale = 1.0, accum = 0.0;
            for (int j = 0; j < octaves; j++)
            {
                int pitch = size >> j;
                int sample1 = (i / pitch) * pitch;
                int sample2 = (sample1 + pitch) % size;
                float blend = float(i - sample1) / float(pitch);
                float sample = (1.0 - blend) * seed[sample1] + blend * seed[sample2];
                noise += sample * scale;
                accum += scale;
                scale /= 2.0;
            }

            noise /= accum;
            image[i] = uint8(mapf(noise, 0, 1, 32, 72));
        }
        
        for (int i = 0; i < int(size); i++)
        {
            float value = seed[i];
            if (value > 1.0)
                value = random::getf();
            else
                value += 0.001;
            seed[i] = value;
        }

        flush();
        resize();
    }
}

int main(string[]@ args)
{
    schedule_policy policy;
    policy.set_threads(4);

    schedule@ queue = schedule::get();
    queue.start(policy);
    
    image_fill main;
    string type = (args.size() > 1 ? args[1] : "matrix");
    if (type == "matrix")
        queue.set_interval(66, task_event(main.loop_matrix));
    else if (type == "noise")
        queue.set_interval(66, task_event(main.loop_noise));
    else if (type == "perlin_1d")
        queue.set_interval(66, task_event(main.loop_perlin_1d));
    else
        queue.stop();
    
    return 0;
}