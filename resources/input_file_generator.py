#NOTE:  This code is written by chatGPT version 4.0 is not my code...
#       I am stating this in case my code is passed through AI text detectors.
#       This generator was not asked by the exercise, I only created it (well GPT did...)
#       For making input files for my Conway's Game of Life Simulation...
#       So this python script is merely a helping tool, for debugging the !@#$ out of the simulation.
#       By the way chatGPT 4.0 is very good at writting tedious python scripts like that... Loving it.


import os
import random

def generate_pattern(rows, cols, alive_density):
    """Generate a random pattern of dead and alive cells based on specified alive density."""
    alive_density = max(0, min(1, float(alive_density)))  # Ensure alive_density is between 0 and 1
    dead_density = 1 - alive_density
    # Modify cell representation to not include the left-side bar in each cell
    return [[random.choices([' ', '*'], weights=[dead_density, alive_density], k=1)[0] for _ in range(cols)] for _ in range(rows)]

def save_pattern(pattern, index, rows, cols):
    """Save the pattern to a file, including dimensions on the first line."""
    filename = f'gpt_python{index}.txt'
    with open(filename, 'w') as file:
        # Write the dimensions on the first line
        file.write(f"{rows} {cols}\n")
        # Write the grid pattern, adjusting how cells are joined
        for row in pattern:
            file.write('|' + '|'.join(row) + '|\n')  # Add bars only at the start and end of each row
    print(f'Pattern saved to {filename}')

def find_next_index():
    """Find the next available index number for the filename."""
    index = 1
    while os.path.exists(f'gpt_python{index}.txt'):
        index += 1
    return index

def main():
    rows = int(input('Enter the number of rows: '))
    cols = int(input('Enter the number of columns: '))
    alive_density = float(input('Enter the density of alive cells (0.0 to 1.0): '))
    pattern = generate_pattern(rows, cols, alive_density)
    index = find_next_index()
    save_pattern(pattern, index, rows, cols)

if __name__ == '__main__':
    main()
