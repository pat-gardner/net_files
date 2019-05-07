import sys

NUM_ARGS = 2

if len(sys.argv) != NUM_ARGS + 1:
    print("Usage: %s <file name> <number of writes>" % sys.argv[0])

filename = sys.argv[1]
max_writes = int(sys.argv[2])

alphabet = "abcdefghijklmnopqrstuvwxyz"

for i in range(max_writes):
    with open(filename, 'a') as f:
        f.write(alphabet)
        

