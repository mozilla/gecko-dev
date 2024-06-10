# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import pathlib
import sys
from datetime import timedelta
from time import strptime


def convert_mem_to_float_mb(mem_val):
    """Parses a string representing a memory value into a float Mb value."""
    if len(mem_val) > 1:
        val = float(mem_val[:-1])
        if mem_val[-1].lower() == "k":
            val /= 1024
    else:
        # 0 values don't have a suffix
        val = float(mem_val)
    return val


args = sys.argv[1:]
cpu_info_file = pathlib.Path(args[0])
binary = args[1]

cpu_info = []
with cpu_info_file.open() as f:
    cpu_info = f.readlines()

# Gather all the final cpu times for the processes
final_times = {}
final_mems = {}
for line in cpu_info:
    if not line.strip():
        continue
    vals = line.split(" ")

    name = vals[0]
    if f"{binary}" not in name:
        # Sometimes the PID catches the wrong process
        continue

    name_split = name.split(f"{binary}:")
    if len(name_split) == 1:
        name = name_split[0]
    else:
        name = name_split[-1]

    final_times[name] = vals[-2]
    final_mems[name] = (vals[1], vals[2])

# Convert the final times to seconds
cpu_time = 0
for name, time in final_times.items():
    dt = strptime(time, "%H:%M:%S")
    seconds = timedelta(
        hours=dt.tm_hour, minutes=dt.tm_min, seconds=dt.tm_sec
    ).total_seconds()
    cpu_time += seconds

# Convert rss, and shr memory to floats, and calculate the pss values
rss_measurements = {"tab": 0, "gpu": 0, "main": 0}
pss_measurements = {"tab": 0, "gpu": 0, "main": 0}
for name, (rss, shr) in final_mems.items():
    rss_val = convert_mem_to_float_mb(rss)
    pss_val = rss_val - convert_mem_to_float_mb(shr)

    final_name = name
    if "tab" in name:
        final_name = "tab"
    elif f"{binary}" in name:
        final_name = "main"

    rss_measurements[final_name] += rss_val
    pss_measurements[final_name] += pss_val

rss_total = sum(list(rss_measurements.values()))
pss_total = sum(list(pss_measurements.values()))

print(
    f"perfMetrics: "
    f'[{{ "name": "cpuTime", "unit": "s", "values": [{cpu_time}] }},'
    f'{{ "name": "rss-memory-total", "unit": "Mb", "values": [{rss_total}] }},'
    f'{{ "name": "rss-memory-tab", "unit": "Mb", "values": [{rss_measurements["tab"]}] }},'
    f'{{ "name": "rss-memory-gpu", "unit": "Mb", "values": [{rss_measurements["gpu"]}] }},'
    f'{{ "name": "rss-memory-main", "unit": "Mb", "values": [{rss_measurements["main"]}] }},'
    f'{{ "name": "pss-memory-total", "unit": "Mb", "values": [{pss_total}] }},'
    f'{{ "name": "pss-memory-tab", "unit": "Mb", "values": [{pss_measurements["tab"]}] }},'
    f'{{ "name": "pss-memory-gpu", "unit": "Mb", "values": [{pss_measurements["gpu"]}] }},'
    f'{{ "name": "pss-memory-main", "unit": "Mb", "values": [{pss_measurements["main"]}] }}]'
)
