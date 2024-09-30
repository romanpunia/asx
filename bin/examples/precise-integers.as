import from
{
    "uint256",
    "console"
};

/*
    An integer class popular in Ethereum blockchain.
*/
[#console::main]
int main()
{
    uint8 max_value_8_manually = 2; // Range: 1 byte
    uint16 max_value_16_manually = 2; // Range: 2 bytes
    uint32 max_value_32_manually = 2; // Range: 4 bytes
    uint64 max_value_64_manually = 2; // Range: 8 bytes
    uint128 max_value_128_manually = 2; // Range: 16 bytes
    uint256 max_value_256_manually = 2; // Range: 32 bytes
    decimal max_value_512_manually = 2; // Range: up available memory bytes
    for (usize i = 0; i < 8; i++)
        max_value_8_manually += (i == 7 ? max_value_8_manually - 1 : max_value_8_manually);
    for (usize i = 0; i < 16; i++)
        max_value_16_manually += (i == 15 ? max_value_16_manually - 1 : max_value_16_manually);
    for (usize i = 0; i < 32; i++)
        max_value_32_manually += (i == 31 ? max_value_32_manually - 1 : max_value_32_manually);
    for (usize i = 0; i < 64; i++)
        max_value_64_manually += (i == 63 ? max_value_64_manually - 1 : max_value_64_manually);
    for (usize i = 0; i < 128; i++)
        max_value_128_manually += (i == 127 ? max_value_128_manually - 1 : max_value_128_manually);
    for (usize i = 0; i < 256; i++)
        max_value_256_manually += (i == 255 ? max_value_256_manually - 1 : max_value_256_manually);
    for (usize i = 0; i < 512; i++)
        max_value_512_manually += (i == 511 ? max_value_512_manually - 1 : max_value_512_manually);

    console@ output = console::get();
    output.write_line("uint8      max: " + to_string(max_value_8_manually));
    output.write_line("uint16     max: " + to_string(max_value_16_manually));
    output.write_line("uint32     max: " + to_string(max_value_32_manually));
    output.write_line("uint64     max: " + to_string(max_value_64_manually));
    output.write_line("uint128    max: " + max_value_128_manually.to_string());
    output.write_line("uint256    max: " + max_value_256_manually.to_string());
    output.write_line("decimal512 max: " + max_value_512_manually.to_string());
    output.write_line("decimal    max: up to " + to_string(max_value_64_manually - 48) + " digits");
    return 0;
}