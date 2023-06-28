function test(value, index)
{
    let shift = 5, zero = 0;
    let hash = index, max = 2 << 29;
    while (value > zero)
        hash = ((hash << shift) - hash + value--) % max;
    return hash;
}
function time()
{
    const hrTime = process.hrtime();
    return hrTime[0] * 1000000 + hrTime[1] / 1000; 
}
function main()
{
    let timing = time();
    if (process.argv.length < 2)
    {
        console.log('provide test sequence index');
        console.log('time: ' + ((time() - timing) / 1000) + "ms");
        return 1;
    }

    let index = parseFloat(process.argv[process.argv.length - 1]);
    if (isNaN(index) || index <= 0)
    {
        console.log('invalid test sequence index');
        console.log('time: ' + ((time() - timing) / 1000) + "ms");
        return 2;
    }

    let value = test((index), 0).toString();
    console.log('result: ' + value);
    console.log('time: ' + ((time() - timing) / 1000) + "ms");
    return 0;
}
process.exit(main());