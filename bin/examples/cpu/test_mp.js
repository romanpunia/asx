const { fork } = require("child_process");
const os = require('os');

function finalize(child, childs)
{
    for (let i = 0; i < childs.length; i++)
    {
        if (childs[i] == child)
        {
            childs.splice(i, 1);
            break;
        }
    }
}
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
async function main()
{
    let timing = time();
    if (process.argv.length < 2)
    {
        console.log('provide test sequence index');
        console.log('time: ' + ((time() - timing) / 1000) + "ms");
        return 1;
    }
    
    if (process.argv[2] == 'fork' && process.argv.length >= 5)
    {
        let data = test(parseFloat(process.argv[3]), parseFloat(process.argv[4]));
        process.send(data.toString());
        return 0;
    }

    let index = parseFloat(process.argv[process.argv.length - 1]);
    if (isNaN(index) || index <= 0)
    {
        console.log('invalid test sequence index');
        console.log('time: ' + ((time() - timing) / 1000) + "ms");
        return 2;
    }

    let hashes = [], processes = [];
    let processes_count = os.cpus().length;
    for (let i = 0; i < processes_count; i++)
    {
        let child = fork(process.argv[1], ['fork', index, i * 4]);
        let close = () => finalize(child, processes);
        child.on('message', (value) => hashes[i] = parseInt(value));
        child.on('exit', close);
        processes.push(child);
    }

    while (processes.length > 0)
        await new Promise((resolve) => setTimeout(resolve, 10));

    for (let i = 0; i < hashes.length; i++)
        console.log("worker result #" + (i + 1) + ": " + hashes[i]);

    console.log('time: ' + ((time() - timing) / 1000) + "ms");
    return 0;
}

main().then((exit_code) => process.exit(exit_code));