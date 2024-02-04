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
    string database = "application";
    
    pdb::host_address address = pdb::host_address::from_url("postgresql://" + username + ":" + password + "@" + hostname + ":5432/" + database + "?connect_timeout=5");
    pdb::cluster@ connection = pdb::cluster();
    try
    {
        /* Throws on connection error */
        co_await connection.connect(address, 1);

        uint64 time = timestamp().milliseconds();
        pdb::cursor cursor = co_await connection.query(args.size() > 1 ? args[1] : "SELECT * FROM pg_catalog.pg_tables;");
        if (cursor.error_or_empty())
            throw exception_ptr("query", "cannot execute a query on a database");

        pdb::response response = cursor.first();
        string[]@ columns = response.get_columns();
        for (usize i = 0; i < columns.size(); i++)
        {
            if (i + 1 < columns.size())
                output.write(columns[i] + " | ");
            else
                output.write_line(columns[i]);
        }

        usize rows = response.size();
        for (usize i = 0; i < rows; i++)
        {
            pdb::row row = response[i];
            output.write("  " + to_string(i + 1) + " (");
            for (usize j = 0; j < columns.size(); j++)
            {
                pdb::column column = row[j];
                schema@ value = column.get_inline();
                string text = (value is null ? "NULL" : value.to_json());
                output.write(j + 1 < columns.size() ? text + ", " : text);
            }
            output.write_line(")");
        }
        output.write_line("returned " + to_string(rows) + " rows in " + to_string(timestamp().milliseconds() - time) + "ms");
    }
    catch
    {
        output.write_line(exception::unwrap().what());
    }

    try
    {
        co_await connection.disconnect(); // If forgotten then connection will be hard reset, throws if already disconnected
    }
    catch { }

    queue.stop();
    return 0;
}