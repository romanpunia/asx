#include "std/console"
#include "std/timestamp"
#include "std/schedule"
#include "std/random"
#include "std/vectors"

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
        point = vector2::randomAbs();
        point.x *= float(width);
        point.y = -point.y * float(height);
        randomizeChar();
    }
    void randomizeChar()
    {
        character = uint8(random::betweeni(32, 72));
    }
}

class image_fill
{
    image_point[]@ points = array<image_point>();
    uint32 x, y, size;
    string image;

    image_fill()
    {
        resize();
    }
    void resize()
    {
        console@ output = console::get();
        uint32 newX, newY;
        output.getSize(newX, newY);

        if (newX == x && newY == y)
            return;

        x = newX;
        y = newY;
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
        output.setCursor(0, 0);
        output.write(image);
        output.flushWrite();
    }
    void loop()
    {
        uint8 empty = ' ';
        for (uint32 i = 0; i < size; i++)
        {
            uint8 color = image[i];
            if (color < empty)
                image[i]++;
            else if (color > empty)
                image[i]--;
        }

        for (usize i = 0; i < points.size(); i++)
        {
            image_point where = points[i];
            points[i].point.y += where.speed;

            int32 height = int32(where.point.y);
            if (height >= y)
            {
                points[i] = image_point(x, y);
                continue;
            }
            else if (height < 0)
                continue;
            else if (height != int32(points[i].point.y))
                points[i].randomizeChar();

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
    policy.setThreads(4);

    schedule@ queue = schedule::get();
    queue.start(policy);

    image_fill main;
    queue.setInterval(50, task_event(main.loop));
    
    return 0;
}