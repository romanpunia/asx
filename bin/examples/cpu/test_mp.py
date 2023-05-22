import time
import sys
import multiprocessing

def test(value, index):
    hash, max = index, 2 << 29
    while value > 0:
        hash = ((hash << 5) - hash + value) % max
        value -= 1
    return hash

def main():
    timing = round(time.time() * 1000)
    if len(sys.argv) < 2:
        print('provide test sequence index')
        print('time: ' + str(round(time.time() * 1000) - timing) + "ms")
        return 1

    try:
        index = int(sys.argv[len(sys.argv) - 1])
        if index <= 0:
            raise index
    except:
        print('invalid test sequence index')
        print('time: ' + str(round(time.time() * 1000) - timing) + "ms")
        return 2

    indices, processes = [], multiprocessing.cpu_count()
    for i in range(0, processes):
        indices.append((index, i * 4))
    
    group, i = multiprocessing.Pool(processes), 1
    for value in group.starmap(test, indices):
        print("worker result #" + str(i) + ": " + str(value))
        i += 1
        
    print('time: ' + str(round(time.time() * 1000) - timing) + "ms")
    return 0

if __name__ == '__main__':
    sys.exit(main())