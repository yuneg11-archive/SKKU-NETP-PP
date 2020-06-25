from matplotlib import pyplot as plt


data = {
    "thr": {},
    "delay": {},
    "trendline": {},
    "interval": {},
    "lost": {},
    "target": {}
}

with open("log.out") as log:
    for line in log:
        tokens = line.split(" ")
        if len(tokens) >= 6:
            if "P)" in tokens[0]:
                continue
            timestamp = float(tokens[0])
            label = tokens[3]
            flow = tokens[4]
            value = float(tokens[5][:-1]) if label == "trendline" else int(tokens[5])
            if flow not in data[label]:
                data[label][flow] = ( [], [] )
            data[label][flow][0].append(timestamp)
            data[label][flow][1].append(value)

sum_thr = 0
max_len = 0
for flow in data["thr"].keys():
    avg_thr = sum(data["thr"][flow][1]) / len(data["thr"][flow][1])
    sum_thr += sum(data["thr"][flow][1])
    max_len = max(max_len, len(data["thr"][flow][1]))
    print(f"Average thr: {flow} - {int(avg_thr)} Kbps")
print(f"Total thr: {int(sum_thr / max_len)} Kbps")
for flow in data["delay"].keys():
    avg_delay = sum(data["delay"][flow][1]) / len(data["delay"][flow][1])
    print(f"Average delay: {flow} - {int(avg_delay)} ms")


plt.figure(figsize=(18, 12))
fig_len = 6
show_legend = False
end = 100

plt.subplot(fig_len, 1, 1)
for flow, flow_data in data["thr"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
plt.ylim(top=10000, bottom=0)
plt.xlabel("Time (sec)")
plt.ylabel("Throughput (Kbps)")
if show_legend:
    plt.legend(data["thr"].keys())

plt.subplot(fig_len, 1, 2)
for flow, flow_data in data["delay"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
plt.xlabel("Time (sec)")
plt.ylabel("Delay (ms)")
if show_legend:
    plt.legend(data["delay"].keys())

plt.subplot(fig_len, 1, 3)
for flow, flow_data in data["trendline"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
plt.ylim(bottom=-0.1, top=0.1)
plt.xlabel("Time (sec)")
plt.ylabel("Trendline")
if show_legend:
    plt.legend(data["trendline"].keys())

plt.subplot(fig_len, 1, 4)
for flow, flow_data in data["interval"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
# plt.ylim(bottom=0, top=500)
plt.xlabel("Time (sec)")
plt.ylabel("Interval (microsec)")
if show_legend:
    plt.legend(data["interval"].keys())

plt.subplot(fig_len, 1, 5)
for flow, flow_data in data["lost"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
# plt.ylim(top=11000)
plt.xlabel("Time (sec)")
plt.ylabel("Lost")
if show_legend:
    plt.legend(data["lost"].keys())

plt.subplot(fig_len, 1, 6)
for flow, flow_data in data["target"].items():
    plt.plot(flow_data[0], flow_data[1])
plt.xlim(left=0, right=end)
# plt.ylim((0, 2000))
plt.xlabel("Time (sec)")
plt.ylabel("Target Interval (microsec)")
if show_legend:
    plt.legend(data["target"].keys())

plt.tight_layout()
plt.savefig("fig.png", dpi=300)
