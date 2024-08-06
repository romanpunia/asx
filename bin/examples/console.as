import from
{
    "console",
    "schedule",
    "random",
    "trigonometry",
    "math"
};

#define REFRESH_RATE 66
#define MIN_CHAR 32
#define MAX_CHAR 72

class image_point
{
    vector2 point;
    uint8 character;
    float speed;

    image_point(uint32 width, uint32 height)
    {
        character = uint8(random::betweeni(MIN_CHAR, MAX_CHAR));
        speed = random::getf();
        point = vector2::random_abs();
        point.x *= float(width);
        point.y = -point.y * float(height);
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
        uint32 new_w, new_h, new_x, new_y;
        output.read_screen(new_w, new_h, new_x, new_y);

        if (new_w == x && new_h == y)
            return;

        x = new_w;
        y = new_h;
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
        output.write_position(0, 0);
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
            where.character = uint8(random::betweeni(MIN_CHAR, MAX_CHAR));
        }

        flush();
        resize();
        schedule::get().set_timeout(REFRESH_RATE, task_async(this.loop_matrix));
    }
    void loop_noise()
    {
        for (uint32 i = 0; i < size; i++)
            image[i] = uint8(random::betweeni(32, 72));

        flush();
        resize();
        schedule::get().set_timeout(REFRESH_RATE, task_async(this.loop_noise));
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
        schedule::get().set_timeout(REFRESH_RATE, task_async(this.loop_perlin_1d));
    }
}

[#console::main]
[#schedule::main(threads = 1)]
int main(string[]@ args)
{
    string type = (args.size() > 1 ? args[1] : "matrix");
    image_fill filler;
    if (type == "matrix")
        filler.loop_matrix();
    else if (type == "noise")
        filler.loop_noise();
    else if (type == "perlin_1d")
        filler.loop_perlin_1d();
    return 0;
}