import from
{
    "schedule",
    "sqlite",
    "console",
    "os",
    "crypto",
    "codec",
    "timestamp",
    "exception"
};

int main(string[]@ args)
{
    console@ output = console::get();
    schedule@ queue = schedule::get();
    queue.start(schedule_policy(2));
    
    string address = "file:///" + __DIRECTORY__ + "/assets/database.db";
    ldb::cluster@ connection = ldb::cluster();
    try
    {
        if (!(co_await connection.connect(address, 1)))
            throw exception_ptr("connect", "cannot connect to a database (url = " + address + ")");

        connection.set_function("random_hex_text", 1, function(array<variant>@ args)
        {
            usize size = usize(args[0].to_uint64());
            string text = codec::hex_encode(crypto::random_bytes(size));
            return var::string_t(text.substring(0, size));
        });

        uint64 time = timestamp().milliseconds();
        ldb::cursor cursor = co_await connection.query(args.size() > 1 ? args[1] : "SELECT random_hex_text(16) AS text_16_bytes, random_hex_text(32) AS text_32_bytes");
        if (cursor.error_or_empty())
            throw exception_ptr("query", "cannot execute a query on a database");

        ldb::response response = cursor.first();
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
            ldb::row row = response[i];
            output.write("  " + to_string(i + 1) + " (");
            for (usize j = 0; j < columns.size(); j++)
            {
                ldb::column column = row[j];
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

    queue.stop();
    return 0;
}