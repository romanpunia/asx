'''
    > python --version
      v2.7.18
    > python examples/cpu/test.py 120000000
      ~19205ms
    > py --version
      v3.11.3
    > py examples/cpu/test.py 120000000
      ~14850ms
'''
import time
import sys

def test(value):
    hash, max = 0, 2 << 29
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

    value = str(test(index))
    print(value)
    print('time: ' + str(round(time.time() * 1000) - timing) + "ms")
    return 0

sys.exit(main())