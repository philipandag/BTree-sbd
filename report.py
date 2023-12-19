import math
import pandas as pd
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

data = pd.read_csv("log.csv")

N = data["N"]
height = data["H"]
nodes = data["nodes"]
IndexReads = data["IR"]
IndexWrites = data["IW"]
IndexFileBytes = data["IB"]
DataReads = data["DR"]
DataWrites = data["DW"]
DataFileBytes = data["DB"]

plt.plot(N, height, label="Height")
plt.xlabel("N")
plt.ylabel("Height")
plt.legend()
plt.show()

plt.plot(N, nodes, label="Nodes")
plt.xlabel("N")
plt.ylabel("amount of nodes")
plt.legend()
plt.show()

plt.plot(N, IndexReads, label="Index reads")
plt.plot(N, IndexWrites, label="Index writes")
plt.xlabel("N")
plt.ylabel("amount of reads/writes")
plt.legend()
plt.show()

plt.plot(N, IndexFileBytes, label="Index file size")
plt.plot(N, DataFileBytes, label="Data file size")
plt.xlabel("N")
plt.ylabel("Bytes")
plt.legend()
plt.show()


plt.plot(N, DataReads, label="Data reads")
plt.plot(N, DataWrites, label="Data writes")
plt.xlabel("N")
plt.ylabel("amount of reads/writes")
plt.legend()
plt.show()


