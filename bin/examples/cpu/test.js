/*
    > node -v
      v20.2.0
    > node examples/cpu/test 120000000
      ~1670ms
*/

function test(value, index)
{
    let shift = 5n, zero = 0n;
    let hash = index, max = 2n << 29n;
    while (value > zero)
        hash = ((hash << shift) - hash + value--) % max;
    return hash;
}
function main()
{
    let timing = new Date().getTime();
    if (process.argv.length < 2)
    {
        console.log('provide test sequence index');
        console.log('time: ' + (new Date().getTime() - timing) + "ms");
        return 1;
    }

    let index = parseFloat(process.argv[process.argv.length - 1]);
    if (isNaN(index) || index <= 0)
    {
        console.log('invalid test sequence index');
        console.log('time: ' + (new Date().getTime() - timing) + "ms");
        return 2;
    }

    let value = test(BigInt(index), 0n).toString();
    console.log('result: ' + value);
    console.log('time: ' + (new Date().getTime() - timing) + "ms");
    return 0;
}
process.exit(main());