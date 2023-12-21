import from
{
    "schedule",
    "postgresql",
    "console",
    "os",
    "timestamp",
    "exception"
};

int main(string[]@ args)
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
    try
    {
        if (!(co_await connection.connect(address, 1)))
            throw exception_ptr("connect", "cannot connect to a database (url = " + address.get_address() + ")");

        uint64 time = timestamp().milliseconds();
        pdb::cursor cursor = co_await connection.query(args.size() > 1 ? args[1] : "SELECT * FROM pg_catalog.pg_tables;");
        if (cursor.error_or_empty())
            throw exception_ptr("query", "cannot execute a query on a database");

        pdb::response response = cursor.first();
        usize rows = response.size();

        output.write_line("response:");
        for (usize i = 0; i < rows; i++)
        {
            pdb::row row = response[i];
            usize columns = row.size();

            output.write_line("  row(" + to_string(i) + "):");
            for (usize j = 0; j < columns; j++)
            {
                pdb::column column = row[j];
                string name = column.get_name();
                schema@ value = column.get_inline();
                string text = (value is null ? "NULL" : value.to_json());
                output.write_line("    " + name + ": " + text);
            }
        }
        output.write_line("\nreturned " + to_string(rows) + " rows in " + to_string(timestamp().milliseconds() - time) + "ms");
    }
    catch
    {
        output.write_line(exception::unwrap().what());
    }

    co_await connection.disconnect(); // If forgotten then connection will be hard reset
    queue.stop();
    return 0;
}