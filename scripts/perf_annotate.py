# run in build directory next to perf.data file

import subprocess
import re
import os
import sys
import argparse

def get_hot_functions(perf_data_file, num_functions):
    """
    Runs 'perf report' and parses the output to find the hottest functions.
    """
    print(f"[*] Analyzing '{perf_data_file}' to find the top {num_functions} functions...")
    try:
        # Command to get the high-level report, ignoring call chain overhead
        cmd = [
            'perf', 'report', '--stdio', '--no-children', 
            '-i', perf_data_file
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True, encoding='utf-8'
        )
    except FileNotFoundError:
        print("\n[!] Error: 'perf' command not found.", file=sys.stderr)
        print("    Please ensure the 'perf' tool (linux-tools-common) is installed and in your PATH.", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        print(f"\n[!] Error running 'perf report':", file=sys.stderr)
        print(e.stderr, file=sys.stderr)
        sys.exit(1)

    functions = []
    pattern = re.compile(r"^\s*\d+\.\d+%.*\s+([\w.:<>~@]+)\s*$")
    
    lines = result.stdout.splitlines()
    
    header_found = False
    for line in lines:
        if 'Overhead' in line and 'Symbol' in line:
            header_found = True
            continue
        if not header_found:
            continue

        match = pattern.match(line)
        if match:
            symbol = match.group(1)
            if ']' not in symbol:
                 functions.append(symbol)
            if len(functions) >= num_functions:
                break
    
    if not functions:
        print("\n[!] Warning: Could not parse any function names from the perf report.")
        print("    Please check the contents of your perf.data file.")

    return functions

def generate_annotation(perf_data_file, function_name, output_dir):
    """
    Runs 'perf annotate' for a given function and saves the output.
    """
    print(f"[*] Generating annotation for '{function_name}'...")
    output_file = os.path.join(output_dir, f'annotate_{function_name}.txt')
    
    try:
        cmd = [
            'perf', 'annotate', function_name, '--stdio', 
            '-i', perf_data_file
        ]
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=True, encoding='utf-8'
        )
    except subprocess.CalledProcessError as e:
        print(f"\n[!] Error annotating function '{function_name}':", file=sys.stderr)
        print(e.stderr, file=sys.stderr)
        return

    with open(output_file, 'w', encoding='utf-8') as f:
        f.write(result.stdout)
    print(f"    -> Saved to '{output_file}'")

def main():
    """
    Main function to drive the performance analysis.
    """
    parser = argparse.ArgumentParser(
        description="Automate 'perf annotate' for the hottest functions in a perf.data file.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        '-i', '--input', default='perf.data',
        help="Path to the perf.data file (default: perf.data)"
    )
    parser.add_argument(
        '-n', '--top', type=int, default=5,
        help="Number of top functions to analyze (default: 5)"
    )
    parser.add_argument(
        '-o', '--output', default='perf_analysis',
        help="Directory to save annotation files (default: perf_analysis)"
    )
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"[!] Error: Input file '{args.input}' not found.", file=sys.stderr)
        sys.exit(1)

    os.makedirs(args.output, exist_ok=True)

    hot_functions = get_hot_functions(args.input, args.top)

    if hot_functions:
        print(f"\n[*] Found top {len(hot_functions)} functions: {', '.join(hot_functions)}")
        for func in hot_functions:
            generate_annotation(args.input, func, args.output)
        print("\n[+] Analysis complete.")
    else:
        print("\n[-] No functions to analyze. Exiting.")


if __name__ == '__main__':
    main()

