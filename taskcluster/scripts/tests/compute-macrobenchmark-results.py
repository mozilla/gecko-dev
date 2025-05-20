import json
import sys


def read_benchmark_data(file_path):
    """Reads the JSON file and returns the benchmark results as a dictionary."""
    with open(file_path) as file:
        data = json.load(file)

    # Extract benchmarks data
    benchmarks = data["benchmarks"]
    results = {}
    for benchmark in benchmarks:
        name = benchmark["name"]
        time_metrics = benchmark["metrics"]["timeToInitialDisplayMs"]
        results[name] = {
            "median": time_metrics["median"],
            "minimum": time_metrics["minimum"],
            "maximum": time_metrics["maximum"],
        }
    return results


def calculate_improvements(results):
    """Calculates percentage improvements between startup with and without baseline profiles."""
    improvements = {
        "median": f"{((results['startupNone']['median'] - results['startupPartialWithBaselineProfiles']['median']) / results['startupNone']['median']) * 100:.2f}%",
        "minimum": f"{((results['startupNone']['minimum'] - results['startupPartialWithBaselineProfiles']['minimum']) / results['startupNone']['minimum']) * 100:.2f}%",
        "maximum": f"{((results['startupNone']['maximum'] - results['startupPartialWithBaselineProfiles']['maximum']) / results['startupNone']['maximum']) * 100:.2f}%",
    }
    return improvements


def format_output_content(results):
    """Formats the output content into the specified JSON structure."""
    # Map to transform result names to subtest entries
    baseline_map = {
        "startupPartialWithBaselineProfiles": "baseline",
        "startupNone": "no_baseline",
    }

    # Construct the subtests list
    subtests = []
    for result_name, metrics in results.items():
        baseline_mode = baseline_map.get(result_name, "unknown")
        for metric_name, value in metrics.items():
            subtest = {
                "name": f"cold_startup.{baseline_mode}.{metric_name}",
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


# Main script logic
if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python script.py <input_json_path> <output_file_path>")
    else:
        input_json_path = sys.argv[1]
        output_file_path = sys.argv[2]

        # Process the benchmark data
        results = read_benchmark_data(input_json_path)
        improvements = calculate_improvements(results)
        output_json = format_output_content(results)
        output_results(output_json, output_file_path)
