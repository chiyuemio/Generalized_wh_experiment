//#include <unistd.h>
//#include <io.h>
#include "task.hpp"
#include "wa.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <time.h>

#include <iostream>
using namespace std;

int main(int argc, char* argv[]) {
    
    unsigned int m = 1;
    unsigned int K = 3;

    int tasksets_count = 100;  // number of task sets
    int tasks_per_set = 5;     // number of tasks per set 

    double total_time = 0.0;   // total time for running
    int total_unschedulable_tasks = 0; // non-schedulable tasks
    int total_satisfies_mk = 0; // tasks meet (m,k)

    for (int set_idx = 1; set_idx <= tasksets_count; set_idx++) {
        
        std::cout << "************************************\n";
        vector<Task> tasks;
        string fname = "5_tasks/taskset_dat/" + to_string(set_idx) + ".dat";
        std::cout << "Reading " << fname << endl;
        tasks = read_a_taskset(fname, tasks_per_set);

        for (int x = 1; x <= tasks_per_set; x++) {
            
            vector<Task> hps;  // tasks with higher priority
            for (int y = 1; y < x; y++)
                hps.push_back(tasks[y - 1]);
            // compute WCRT、BP、BCRT
            compute_wcrt(tasks[x - 1], hps);
            compute_bp(tasks[x - 1], hps);
            compute_bcrt(tasks[x - 1], hps);

            
            if (tasks[x - 1].wcrt <= tasks[x - 1].dline)
                continue;  // schedulable task
            std::cout << "------------------------------------\n";
            tasks[x-1].print();
            // std::cout << "Task " << tasks[x - 1].get_id() << " is un-schedulable\n";
            std::cout << "Task " << x << " is un-schedulable\n";

            // non-schedulable task, weakly hard analysis
	        std::cout << "Running weakly-hard analysis: " << "m=" << m << ", K=" << K << " ... " << std::endl;

            if (tasks[x - 1].wcrt > 10000){
                std::cout << "taskset" << to_string(set_idx) << " task" << x << ": wcrt is not convergent..." << endl;
                std::cout << endl;
                continue;
            }

            total_unschedulable_tasks++;  
            statWA swa;
            try{
                swa = wanalysis(tasks[x - 1], hps, m, K);
            }catch(...){
            }
              
            std::cout << "the number of missed deadline is " << swa.m << "\n";
	        std::cout << "Time = " << swa.time << " sec " << std::endl;
	        std::cout << "Constrain satisfied? " << swa.satisfies_mk << "\n";
	        std::cout << std::endl << std::endl;

            total_time += swa.time;
            
            if (swa.satisfies_mk) {
                total_satisfies_mk++;
            }
        }
    }

    double average_time = total_time / (tasksets_count * tasks_per_set);
    double mk_satisfaction_ratio = (total_unschedulable_tasks == 0) ? 0.0 : (double)total_satisfies_mk / total_unschedulable_tasks;

    std::cout << "total_satisfies_mk is " << total_satisfies_mk << "\n";
    std::cout << "total_unschedulable_tasks is " << total_unschedulable_tasks << "\n";
    cout << "Average execution time: " << average_time << " sec" << endl;
    cout << "Ratio of unschedulable tasks that satisfy m/K: " << mk_satisfaction_ratio * 100 << "%" << endl;

    system("pause");
    return 0;
}