import os
import argparse
import numpy as np

def convert(filename, weight_gen):
    with open(f"{filename}.cnf", "r") as f:
        lines = f.readlines()
    
    n_clauses = 0
    weights = None
    init_idx = -1
    
    for i in range(len(lines)):
        line = lines[i]
        if line.startswith('c'): 
            continue
        elif line.startswith('p'):
            parts = line.split(' ')
            assert parts[0] == 'p', parts
            
            n_clauses = int(parts[-1])
            if weight_gen == "rand":
                weights = np.random.rand(n_clauses)
            elif weight_gen == "equal":
                weights = np.ones(n_clauses)
            elif weight_gen == "desc":
                weights = np.linspace(1.0, 0.0, n_clauses, endpoint=False)
        else:
            assert n_clauses != 0, line
            assert weights is not None, weights

            if init_idx == -1:
                init_idx = i
                
            parts = line.split(' ')
            parts.insert(0, str(weights[i - init_idx]))
            
            lines[i] = ' '.join(parts)
            
    
    with open(f"{filename}_{weight_gen}.wcnf", "w") as f:
        f.writelines(lines)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument('--data', '-d', type=str, default='todo')
    parser.add_argument('--weight_gen', '-w', type=str, default='rand', choices=['rand', 'desc', 'equal'])
    
    args = vars(parser.parse_args())
    
    for filename in os.listdir(args['data']):
        if filename.endswith('.cnf'):
            full_path = os.path.join(args['data'], filename)
            convert('.'.join(full_path.split('.')[:-1]), args['weight_gen'])

if __name__ == '__main__':
    main()