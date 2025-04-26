import from
{
    "exception",
    "crypto",
    "codec",
    "console"
};

/*
    ECDSA, EDDSA, RSA and DSA examples
*/
void output_result(uptr@ digest, int32 proof, int32 curve, const secret_box&in private_key, const string&in public_key, const string&in message, const string&in signature, bool valid)
{
    schema@ result = var::set::object_t();
    schema@ algorithm = result.set("algorithm", var::set::object_t());
    algorithm.set("type", var::string_t(crypto::get_proof_name(proof)));
    algorithm.set("order", var::string_t(crypto::get_curve_name(curve)));
    algorithm.set("hash", var::string_t(crypto::get_digest_name(@digest)));
    schema@ keypair = result.set("keypair", var::set::object_t());
    keypair.set("private_key", var::string_t(codec::hex_encode(private_key.heap())));
    keypair.set("public_key", var::string_t(codec::hex_encode(public_key)));
    schema@ signing = result.set("signing", var::set::object_t());
    signing.set("message", var::string_t(message));
    signing.set("signature", var::string_t(codec::hex_encode(signature)));
    signing.set("valid", var::boolean_t(true));
    console::get().jwrite_line(@result);
}
void test_ecdsa(uptr@ digest, int32 curve, const string&in message)
{
    try
    {
        secret_box private_key = crypto::ec_private_key_gen(curve);
        string public_key = crypto::ec_to_public_key(curve, proofs::format::compressed, private_key);
        string signature = crypto::ec_sign(@digest, curve, message, private_key);
        bool valid = crypto::ec_verify(@digest, curve, message, signature, public_key);
        output_result(@digest, 0, curve, private_key, public_key, message, crypto::ec_der_to_rs(signature), valid);
    }
    catch
    {
        console::get().write_line("ecdsa error: " + exception::unwrap().what());
    }
}
void test_dsa(uptr@ digest, int32 proof, const string&in message)
{
    try
    {
        secret_box private_key = crypto::private_key_gen(proof);
        string public_key = crypto::to_public_key(proof, private_key);
        string signature = crypto::sign(@digest, proof, message, private_key);
        bool valid = crypto::verify(@digest, proof, message, signature, public_key);
        output_result(@digest, proof, 0, private_key, public_key, message, signature, valid);
    }
    catch
    {
        console::get().write_line("dsa error: " + exception::unwrap().what());
    }
}

[#console::main]
int main()
{
    string message = "Hello, World!";
    test_ecdsa(digests::sha256(), proofs::curves::secp256k1(), message);
    test_dsa(null, proofs::ed25519(), message);
    test_dsa(digests::sha256(), proofs::rsa(), message);
    test_dsa(digests::sha256(), proofs::dsa(), message);
    return 0;
}