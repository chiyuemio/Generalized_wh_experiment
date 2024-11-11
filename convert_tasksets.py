import os

    # Function to read the taskset from a txt file
def read_taskset_from_txt(filename):
    tasksets = []
    with open(filename, 'r') as file:
        for line in file:
        # Remove braces and split by comma
            line = line.strip().replace('{', '').replace('}', '').replace(';', '').strip()
            if line:
                task = tuple(map(lambda x: int(x) if x.isdigit() else x == 'true', line.split(',')))
                tasksets.append(task)
    return tasksets

    # Function to extract wcet, deadline, and period from taskset
def extract_taskset_fields(tasksets):
    return [(wcet, deadline, period) for _, wcet, period, deadline, *rest in tasksets]

    # Function to save the new taskset to .dat files
def save_taskset_to_dat_files(taskset, count, n, u):
    filename = 'taskset_phase2/n-' + str(n) + '_u-' + str(u) + '/'+ str(count) + '.dat'
    dir_path = os.path.dirname(filename)
    if not os.path.exists(dir_path):
        os.makedirs(dir_path)
    with open(filename, 'w') as f:
        for wcet, deadline, period in taskset:
            f.write(f'{wcet} {deadline} {period}\n')
    print(f'Saved {filename}')

    # Main function
def main():

    n = 7
    u = 0.99
    for i in range(1, 101):
        input_filename = 'taskset/n-' + str(n) + '_u-' + str(u) + '/' + str(i) + '.txt'
        tasksets = read_taskset_from_txt(input_filename)
        new_taskset = extract_taskset_fields(tasksets)
        save_taskset_to_dat_files(new_taskset, i, n, u)

if __name__ == "__main__":
    main()