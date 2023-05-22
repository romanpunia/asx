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
    let hash = index, max = 2n << 29n;
    while (value > 0)
        hash = ((hash << 5n) - hash + value--) % max;
    return hash;
}
async function main()
{
    let timing = new Date().getTime();
    if (process.argv.length < 2)
    {
        console.log('provide test sequence index');
        console.log('time: ' + (new Date().getTime() - timing) + "ms");
        return 1;
    }
    
    if (process.argv[2] == 'fork' && process.argv.length >= 5)
    {
        let data = test(BigInt(process.argv[3]), BigInt(process.argv[4]));
        process.send(data.toString());
        return 0;
    }

    let index = parseFloat(process.argv[process.argv.length - 1]);
    if (isNaN(index) || index <= 0)
    {
        console.log('invalid test sequence index');
        console.log('time: ' + (new Date().getTime() - timing) + "ms");
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

    console.log('time: ' + (new Date().getTime() - timing) + "ms");
    return 0;
}

main().then((exit_code) => process.exit(exit_code));