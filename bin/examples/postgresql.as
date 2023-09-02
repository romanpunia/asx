#include <std/schedule.as>
#include <std/postgresql.as>
#include <std/console.as>
#include <std/os.as>
#include <std/timestamp.as>

int main()
{
    console@ output = console::get();
    schedule@ queue = schedule::get();
    queue.start(schedule_policy(2));
    
    string hostname = os::process::get_env("DB_ADDRESS");
    string username = os::process::get_env("DB_USERNAME");
    string password = os::process::get_env("DB_PASSWORD");
    string table = "application";
    
    pdb::host_address address("postgresql://" + username + ":" + password + "@" + hostname + ":5432/" + table + "?connect_timeout=5");
    pdb::cluster@ connection = pdb::cluster();
    if (!(co_await connection.connect(address, 1)))
    {
        output.write_line("cannot connect to database");
        queue.stop();
        return 1;
    }

    uint64 time = timestamp().milliseconds();
    pdb::cursor cursor = co_await connection.query("SELECT * FROM pg_catalog.pg_tables;");
    if (cursor.error_or_empty())
    {
        output.write_line("cannot query database");
        co_await connection.disconnect();
        queue.stop();
        return 2;
    }

    pdb::response response = cursor.first();
    usize rows = response.size();

    string content = "response:\n";
    for (usize i = 0; i < rows; i++)
    {
        pdb::row row = response[i];
        usize columns = row.size();

        content += "  row(" + to_string(i) + "):\n";
        for (usize j = 0; j < columns; j++)
        {
            pdb::column column = row[j];
            string name = column.get_name();
            schema@ value = column.get_inline();
            string text = (value is null ? "NULL" : value.to_json());
            content += "    " + name + ": " + text + "\n";
        }
    }

    content += "\nreturned " + to_string(rows) + " rows in " + to_string(timestamp().milliseconds() - time) + "ms";
    output.write_line(content);

    co_await connection.disconnect(); // If forgotten then connection will be hard reset
    queue.stop();
    return 0;
}