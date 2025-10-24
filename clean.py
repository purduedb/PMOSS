#!/usr/bin/env python3
"""
Script to synchronize rows across multiple data collection folders.
For each file (e.g., query-exec_view.txt), finds rows that are common across
ALL folders (2/, 14/, 26/, etc.) and keeps only those common rows.
"""

from pathlib import Path
from typing import List, Dict, Tuple
# write a class like thing to store filter criteria
class RowFilter:
    def __init__(self, round, wl, cfg, cnt):
        self.round = round
        self.wl = wl
        self.cfg = cfg
        self.cnt = cnt


def parse_row_key(row: str, num_values: int = 5) -> Tuple:
    """
    Extract the first num_values from a row to use as a key.

    Args:
        row: A row string with space-separated values
        num_values: Number of values to extract for the key (default: 5)

    Returns:
        Tuple of the first num_values as the key
    """
    parts = row.strip().split()
    if len(parts) < num_values:
        return tuple(parts)
    return tuple(parts[:num_values])


def read_file_with_keys(file_path: Path, num_key_values: int = 5) -> Dict[Tuple, str]:
    """
    Read a file and create a dictionary mapping keys to full rows.

    Args:
        file_path: Path to the file
        num_key_values: Number of values to use for the key

    Returns:
        Dictionary mapping row keys to full row content
    """
    rows = {}
    if not file_path.exists():
        return rows

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                key = parse_row_key(line, num_key_values)
                rows[key] = line

    return rows


def find_common_rows_across_folders(folder_path: Path, filename: str, num_key_values: int = 5) -> Dict[Tuple, str]:
    """
    Find common rows across all subfolders for a specific filename.

    For example, finds rows common to:
    - 2/query-exec_view.txt
    - 14/query-exec_view.txt
    - 26/query-exec_view.txt
    - etc.

    Args:
        folder_path: Path to the main folder (e.g., kb_bs_dynam)
        filename: Name of the file to process (e.g., query-exec_view.txt)
        num_key_values: Number of values to use for the key

    Returns:
        Dictionary of common rows with their keys
    """
    subfolders = sorted([d for d in folder_path.iterdir() if d.is_dir()])

    if not subfolders:
        print(f"No subfolders found in {folder_path}")
        return {}

    print(f"\nProcessing {filename} across all folders...")

    # Read all files and collect their row data
    all_folder_data = {}
    all_keys_sets = []
    first_folder_keys_ordered = []  # Track insertion order from first folder

    for i, subfolder in enumerate(subfolders):
        file_path = subfolder / filename
        if file_path.exists():
            file_data = read_file_with_keys(file_path, num_key_values)
            all_folder_data[subfolder.name] = file_data
            all_keys_sets.append(set(file_data.keys()))

            # Save the order from the first folder
            if i == 0:
                first_folder_keys_ordered = list(file_data.keys())

            print(f"  {subfolder.name}/{filename}: {len(file_data)} rows")
        else:
            print(f"  {subfolder.name}/{filename}: NOT FOUND")

    if not all_keys_sets:
        print(f"  ERROR: No files found for {filename}")
        return {}

    # Find intersection of all keys (common rows across all folders)
    common_keys_set = set.intersection(*all_keys_sets)
    print(f"  → Common rows across all folders: {len(common_keys_set)}")

    if not common_keys_set:
        print(f"  WARNING: No common rows found!")
        return {}


    # Build the common rows dictionary using the first folder's data
    # Keep only keys that are common, in the original order from first folder
    common_rows = {}
    first_folder = subfolders[0].name
    if first_folder in all_folder_data:
        for key in first_folder_keys_ordered:
            if key in common_keys_set:
                common_rows[key] = all_folder_data[first_folder][key]

    return common_rows


def write_synchronized_files(input_folder: Path, output_folder: Path, filename: str,
                             common_keys: list, num_key_values: int = 5):
    """
    Write files with only the common keys to each subfolder in the output folder.
    Each folder keeps its own data for the common keys.

    Args:
        input_folder: Path to the input folder
        output_folder: Path to the output folder
        filename: Name of the file to write
        common_keys: Ordered list of common keys across all folders
        num_key_values: Number of values used for the key
    """
    if not common_keys:
        print(f"  No rows to write for {filename}")
        return

    input_subfolders = sorted([d for d in input_folder.iterdir() if d.is_dir()])

    print(f"\nWriting synchronized {filename}...")
    for subfolder in input_subfolders:
        # Read this folder's data
        input_file = subfolder / filename
        folder_data = read_file_with_keys(input_file, num_key_values)

        # Create output subfolder if it doesn't exist
        output_subfolder = output_folder / subfolder.name
        output_subfolder.mkdir(parents=True, exist_ok=True)

        output_file = output_subfolder / filename

        # Write only the common keys with THIS folder's data, preserving order
        with open(output_file, 'w') as f:
            for key in common_keys:
                if key in folder_data:
                    f.write(folder_data[key] + '\n')

        print(f"  ✓ {output_subfolder.name}/{filename}: {len(common_keys)} rows")


def synchronize_folders(input_folder: str, output_folder: str,
                       filenames: List[str] = None, num_key_values: int = 5,
                       filter_criteria: RowFilter = None):
    """
    Synchronize multiple files across all subfolders.

    Args:
        input_folder: Path to the input folder (e.g., 'kb_bs_dynam')
        output_folder: Path to the output folder (e.g., 'kb_bs_dynam_clean')
        filenames: List of filenames to synchronize. If None, uses default list.
        num_key_values: Number of values to use for the key (default: 5)
    """
    input_path = Path(input_folder)
    output_path = Path(output_folder)

    if not input_path.exists():
        print(f"ERROR: Input folder not found: {input_folder}")
        return

    # Create output folder
    output_path.mkdir(parents=True, exist_ok=True)

    if filenames is None:
        filenames = ['query-exec_view.txt', 'data_view.txt', 'mem-channel_view.txt']

    print("=" * 80)
    print(f"SYNCHRONIZATION TASK")
    print(f"  Input folder:  {input_folder}")
    print(f"  Output folder: {output_folder}")
    print(f"  Files:         {', '.join(filenames)}")
    print(f"  Row key:       First {num_key_values} values")
    print("=" * 80)

    # Store the common keys from query-exec_view.txt to use for mem-channel_view.txt
    reference_common_keys = None

    for filename in filenames:
        # Special handling for mem-channel_view.txt: use reference keys from query-exec_view.txt
        if filename == 'mem-channel_view.txt' and reference_common_keys is not None:
            print(f"\nProcessing {filename} (only in folder 2, matching keys from query-exec_view.txt)...")

            # Read only from folder 2
            folder_2_path = input_path / '2' / filename
            if folder_2_path.exists():
                folder_2_data = read_file_with_keys(folder_2_path, num_key_values)
                print(f"  2/{filename}: {len(folder_2_data)} rows")

                # Filter to only keys that match reference_common_keys
                common_rows = {}
                for key in reference_common_keys:
                    if key in folder_2_data:
                        common_rows[key] = folder_2_data[key]

                print(f"  → Rows matching query-exec_view keys: {len(common_rows)}")
                common_keys = list(common_rows.keys())
            else:
                print(f"  ERROR: 2/{filename} not found!")
                common_keys = []
        else:
            # Find common rows across all folders for this file
            common_rows = find_common_rows_across_folders(input_path, filename, num_key_values)

            if common_rows:
                # Extract the keys from common_rows, preserving insertion order
                common_keys = list(common_rows.keys())

                # Save reference keys from query-exec_view.txt for later use with mem-channel_view.txt
                if filename == 'query-exec_view.txt':
                    reference_common_keys = common_keys.copy()
            else:
                common_keys = []

        if common_keys:
            # Add filtering here if needed (e.g., only keys with specific criteria)
            if filter_criteria:
                filtered_keys = []
                for key in common_keys:
                    if (int(key[0]) == filter_criteria.cfg and
                        int(key[2]) == filter_criteria.wl and
                        int(key[4]) == filter_criteria.round):
                        filtered_keys.append(key)
                # Use the count to get only the first 'cnt' entries
                if filter_criteria.cnt != -1:
                    filtered_keys = filtered_keys[:filter_criteria.cnt]
                common_keys = filtered_keys
                print(f"  → After filtering: {len(common_keys)} rows match criteria")

                # Update reference keys after filtering if this is query-exec_view.txt
                if filename == 'query-exec_view.txt':
                    reference_common_keys = common_keys.copy()

            print(common_keys[:5])  # Print first 5 common keys for verification

            # Special handling for mem-channel_view.txt: only write to folder 2
            if filename == 'mem-channel_view.txt':
                output_subfolder = output_path / '2'
                output_subfolder.mkdir(parents=True, exist_ok=True)
                output_file = output_subfolder / filename

                folder_2_path = input_path / '2' / filename
                if folder_2_path.exists():
                    folder_2_data = read_file_with_keys(folder_2_path, num_key_values)
                    with open(output_file, 'w') as f:
                        for key in common_keys:
                            if key in folder_2_data:
                                f.write(folder_2_data[key] + '\n')
                    print(f"\n  ✓ 2/{filename}: {len(common_keys)} rows")
            else:
                # Write synchronized files to output folder for all folders
                write_synchronized_files(input_path, output_path, filename, common_keys, num_key_values)
        else:
            print(f"  Skipping {filename} - no common rows found")

    print("\n" + "=" * 80)
    print(f"COMPLETE! Synchronized files written to: {output_folder}")
    print("=" * 80)


def main():
    """Main function to run the synchronization."""
    import sys

    if len(sys.argv) < 3:
        print("Usage: python clean.py <input_folder> <output_folder> [num_key_values]")
        print("\nExample:")
        print("  python clean.py kb_bs_dynam kb_bs_dynam_clean")
        print("  python clean.py kb_bs_dynam kb_bs_dynam_clean 5")
        print("\nThis will:")
        print("  - Find common rows across all subfolders (2/, 14/, 26/, etc.)")
        print("  - For each file type (query-exec_view.txt, data_view.txt, mem-channel_view.txt)")
        print("  - Write synchronized files to the output folder")
        sys.exit(1)

    input_folder = sys.argv[1]
    output_folder = sys.argv[2]
    num_key_values = int(sys.argv[3]) if len(sys.argv) > 3 else 5
    filter_criteria = RowFilter(
        round=74, 
        wl=12, 
        cfg=506, 
        cnt=-1
        )
    synchronize_folders(input_folder, output_folder, num_key_values=num_key_values,
                        filter_criteria=filter_criteria)


if __name__ == "__main__":
    main()
