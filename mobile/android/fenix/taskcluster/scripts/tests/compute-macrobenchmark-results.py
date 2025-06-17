import json
import sys
from collections import defaultdict
from os import listdir
from os.path import isfile, join


def read_benchmark_data_from_directory(directory):
    ## org.mozilla.fenix.benchmark-benchmarkData.json
    benchmark_files = [
        file for file in listdir(directory) if isfile(join(directory, file))
    ]
    benchmark_results = {}
    for benchmark_file in benchmark_files:
        read_benchmark_data(f"{directory}/{benchmark_file}", benchmark_results)

    return benchmark_results


def read_benchmark_data(file_path, results):
    """Reads the JSON file and returns the benchmark results as a dictionary."""
    with open(file_path) as file:
        data = json.load(file)

    # Extract benchmarks data
    benchmarks = data["benchmarks"]
    for benchmark in benchmarks:
        name = benchmark["name"]
        time_metrics = benchmark["metrics"]["timeToInitialDisplayMs"]
        results[name] = {
            "median": time_metrics["median"],
            "minimum": time_metrics["minimum"],
            "maximum": time_metrics["maximum"],
        }
    return results


def format_output_content(results):
    """Formats the output content into the specified JSON structure."""

    # Construct the subtests list
    subtests = []
    for result_name, metrics in results.items():
        for metric_name, value in metrics.items():
            subtest = {
                "name": f"{result_name}.{metric_name}",
                "lowerIsBetter": True,
                "value": value,
                "unit": "ms",
            }
            subtests.append(subtest)

    # Define the base JSON structure using the subtests list
    output_json = {
        "framework": {"name": "mozperftest"},
        "application": {"name": "fenix"},
        "suites": [
            {
                "name": "baseline-profile:fenix",
                "type": "coldstart",
                "unit": "ms",
                "extraOptions": [],
                "lowerIsBetter": True,
                "subtests": subtests,
            }
        ],
    }

    return output_json


def output_results(output_json, output_file_path):
    """Writes the output JSON to a specified file and prints it in a compacted format to the console."""
    # Convert JSON structure to a compacted one-line string
    compact_json = json.dumps(output_json)

    # Print in the specified format
    print(f"PERFHERDER_DATA: {compact_json}")

    # Write the pretty-formatted JSON to the file
    with open(output_file_path, "w") as output_file:
        output_file.write(json.dumps(output_json, indent=3))
    print(f"Results have been written to {output_file_path}")


def generate_markdown_table(results):
    # Step 1: Organize the data
    table_data = defaultdict(lambda: {"median": None, "median None": None})

    for name, metrics in results.items():
        base_name = name.replace("PartialWithBaselineProfiles", "")
        if base_name.endswith("None"):
            main_name = base_name[:-4]
            table_data[main_name]["median None"] = metrics["median"]
        else:
            table_data[base_name]["median"] = metrics["median"]

    # Step 2: Prepare markdown rows
    headers = ["Benchmark", "median", "median None", "% diff"]
    lines = [
        f"| {' | '.join(headers)} |",
        f"|{':-' + '-:|:-'.join(['-' * len(h) for h in headers])}-:|",
    ]

    for benchmark, values in sorted(table_data.items()):
        median = values["median"]
        median_none = values["median None"]
        if median is not None and median_none:
            percent_diff = round((median_none - median) / median_none * 100, 1)
        else:
            percent_diff = ""

        row = f"| {benchmark} | {median:.3f} | {median_none:.3f} | {percent_diff} |"
        lines.append(row)

    return "\n".join(lines)


# Main script logic
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python script.py <input_json_path> <output_file_path>")
    else:
        input_json_path = sys.argv[1]
        output_file_path = sys.argv[2]

        # Process the benchmark data
        results = read_benchmark_data_from_directory(input_json_path)
        print(generate_markdown_table(results))
        output_json = format_output_content(results)
        output_results(output_json, output_file_path)
