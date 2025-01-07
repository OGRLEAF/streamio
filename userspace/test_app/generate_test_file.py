import random
SIZE = 256 * 1024
# open("test_wave_file.txt", "w+").writelines([f"{int(random.random() * (2 ** 16 - 1))}\n" for _ in range(0, SIZE)])
open("test_wave_file.txt", "w+").writelines([f"{int(_)}\n" for _ in range(0, SIZE)])