max = 1000

# open file for writing
f = open("sequential.txt", "w")

for i in range(1, max+1):
    f.write("+" + str(i) + "\n")

for i in range(max, 0, -1):
    f.write("-" + str(i) + "\n")