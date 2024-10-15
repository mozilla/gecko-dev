import sys
from numpy.random import randint;

ARGV = sys.argv
FILENAME = ARGV[1] if len(ARGV) >= 2 else "input.txt"

MAX = (1 << 24)
LEN = 2000

file = open(FILENAME, "w")
file.write(str(LEN) + "\n")
for num in range(LEN):
    file.write(str(randint(0, MAX)) + "\n")

file.close()

print("Generate {} numbers within range [0, {}].".format(LEN, MAX))
