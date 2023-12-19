import math
import pandas as pd
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

NMdata = pd.read_csv("NaturalMergeSort-Random.benchmark.csv")
PPdata = pd.read_csv("PolyphaseMergeSort-Random.benchmark.csv")
revNMdata = pd.read_csv("NaturalMergeSort-Reversed.benchmark.csv")
revPPdata = pd.read_csv("PolyphaseMergeSort-Reversed.benchmark.csv")


#print(NMdata)
#print(PPdata)

# mpl.rcParams['axes.prop_cycle'] = mpl.cycler(color=["r", "k", "c"]) 

plt.title("Porównanie faz sortowania Polyphase i Natural merge")
plt.ylabel("Fazy sortowania")
plt.xlabel("N")

N = NMdata["N"]
nmp = NMdata["Phases"]
revnmp = revNMdata["Phases"]
plt.plot(N, nmp, label="Natural Merge")

N = PPdata["N"]
ppp = PPdata["Phases"]
revppp = revPPdata["Phases"]
plt.plot(N, ppp, label="Polyphase Merge")

plt.legend()
plt.savefig("porownanie_fazy.png")
plt.show()


plt.title("Porównanie operacji dyskowych Polyphase i Natural merge")
plt.ylabel("Operacje dyskowe")
plt.xlabel("N")

N = NMdata["N"]
nm = NMdata["DiskOperations"]
revnm = revNMdata["DiskOperations"]
plt.plot(N, nm, label="Natural Merge")

N = PPdata["N"]
pp = PPdata["DiskOperations"]
revpp = revPPdata["DiskOperations"]
plt.plot(N, pp, label="Polyphase Merge")

plt.legend()
plt.savefig("porownanie_operacje.png")
plt.show()


b = 5 # 5 rekordow na blok, upewnic sie ze w programie jest to samo


plt.title("Natural Merge\nPorównanie ilości faz sortowania z teoretyczną spodziewaną")
plt.ylabel("Fazy sortowania")
plt.xlabel("N")
N = NMdata["N"]
r = N/2 # spodziewana (Srednia) ilosc serii

teoretyczne = [math.ceil(math.log2(r[i])) for i in range(len(N))]
plt.plot(N, teoretyczne, '--', label="średnia: ceil( log2(r) )", linewidth=3)
teoretyczne = [math.ceil(math.log2(N[i])) for i in range(len(N))]
plt.plot(N, nmp, label="zmierzona: losowe pliki")

plt.plot(N, teoretyczne, '--', label="max: ceil( log2(N) )", linewidth=3)
plt.plot(N, revnmp, label="zmierzona: odwrotnie posortowane pliki")

plt.legend()
plt.savefig("natural_fazy.png")
plt.show()


plt.title("Natural Merge\nPorównanie ilości operacji dyskowych z wartościami teoretycznymi")
plt.ylabel("Operacje dyskowe")
plt.xlabel("N")
N = NMdata["N"]
r = N/2 # spodziewana (Srednia) ilosc serii

teoretyczne = [4*N[i] * math.ceil(math.log2(r[i]))/b for i in range(len(N))]
plt.plot(N, teoretyczne, '--', label="średnia: 4N * ceil( log2(r) ) / b", linewidth=3)
plt.plot(N, nm, label="zmierzona: losowe pliki")

teoretyczne = [4*N[i] * math.ceil(math.log2(N[i]))/b for i in range(len(N))]
plt.plot(N, teoretyczne, '--', label="max: 4N * ceil( log2(N) ) / b", linewidth=3)
plt.plot(N, revnm, label="zmierzona: odwrotnie posortowane pliki")

plt.legend()
plt.savefig("natural_operacje.png")
plt.show()


plt.title("Polyphase Merge\nPorównanie ilości faz sortowania z wartościami teoretycznymi")
plt.ylabel("Fazy sortowania")
plt.xlabel("N")
N = PPdata["N"]
r = N/2 # spodziewana (Srednia) ilosc serii

teoretyczne = [1.45 * math.log2(r[i]) for i in range(len(N))]
plt.plot(N, teoretyczne, '--', label="średnia: 1.45log2(r)", linewidth=3)
plt.plot(N, ppp, label="zmierzona: losowe pliki")
plt.plot(N, revppp, label="zmierzona: odwrotnie posortowane pliki")

plt.legend()
plt.savefig("polyphase_fazy.png")
plt.show()


plt.title("Polyphase Merge\nPorównanie ilości operacji dyskowych z wartościami teoretycznymi")
plt.ylabel("Operacje dyskowe")
plt.xlabel("N")
N = PPdata["N"]
r = N/2 # spodziewana (Srednia) ilosc serii

teoretyczne = [2*N[i] * (1.04 * math.log2(r[i]) + 1) / b for i in range(len(N))]
plt.plot(N, teoretyczne, '--', label="średnia: 2N( 1.04 * log2(r) + 1) / b", linewidth=3)
plt.plot(N, pp, label="zmierzona: losowe pliki")
plt.plot(N, revpp, label="zmierzona: odwrotnie posortowane pliki")

plt.legend()
plt.savefig("polyphase_operacje.png")
plt.show()