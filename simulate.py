import os
import serial
import time

def clear():
    os.system('cls' if os.name == 'nt' else 'clear')

def print_rhombus(matrix):

    n = len(matrix)
    
    # Function to flatten the matrix
    def flatten(matrix):
        return [matrix[i][j] for i in range(n) for j in range(n)]

    flattened_matrix = flatten(matrix)
    index = 0
    
    # Print the upper part of the rhombus
    i = 0
    j = 0
    while i + j <= n - 1:
        r = i
        c = j
        print('  ' * (n - i - 1), end='')
        while r >= j and c <= i:
            print(flattened_matrix[r * n + c], end='   ')
            r -= 1
            c += 1
        print()
        if i >= n - 1: j += 1
        else: i += 1 
    i = n - 1
    j = 1
    while i + j <= n * 2 - 2:
        r = i
        c = j
        print('  ' * (j), end='')
        while r >= j and c <= i:
            print(flattened_matrix[r * n + c], end='   ')
            r -= 1
            c += 1
        print()
        if i >= n - 1: j += 1
        else: i += 1 

ser = serial.Serial("COM3", 9600)
def read_matrix(rows, cols):
    matrix = []

    while len(matrix) < rows:
        line = ser.readline().decode('utf-8').strip()
        if line:
            row_data = line.split(';')
            for row_str in row_data:
                if row_str:
                    row = [int(val) for val in row_str.split(',')]
                    if len(row) == cols:
                        matrix.append(row)
    
    return matrix

while True:
    m = read_matrix(5, 5)
    m2 = read_matrix(5, 5)
    clear()
    print_rhombus(m)
    print_rhombus(m2)
    

# Example usage
matrix = [
    [1, 2, 3, 4],
    [5, 6, 7, 8],
    [9, 0, 1, 2],
    [3, 4, 5, 6]
]
matrix2 = [
    [1, 2, 3, 4],
    [5, 6, 7, 8],
    [9, 0, 1, 2],
    [3, 4, 5, 6]
]

print_rhombus(matrix)
print_rhombus(matrix2)
