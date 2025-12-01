#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_EXAMS 20
#define NUM_QUESTIONS 5
#define SHM_SIZE 1024

typedef struct {
    char rubric[NUM_QUESTIONS][100];
    int current_exam;
    int questions_marked[NUM_QUESTIONS];
    char exam_content[100];
} shared_data;

void load_rubric(shared_data *data) {
    FILE *file = fopen("rubric.txt", "r");
    if (file) {
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            fgets(data->rubric[i], 100, file);
            // Remove newline if present
            data->rubric[i][strcspn(data->rubric[i], "\n")] = 0;
        }
        fclose(file);
    }
}

void load_exam(shared_data *data, int exam_num) {
    char filename[20];
    snprintf(filename, sizeof(filename), "exam_%02d.txt", exam_num);
    
    FILE *file = fopen(filename, "r");
    if (file) {
        fgets(data->exam_content, 100, file);
        data->exam_content[strcspn(data->exam_content, "\n")] = 0;
        fclose(file);
    }
    
    // Reset marked questions
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        data->questions_marked[i] = 0;
    }
    
    data->current_exam = exam_num;
}

void ta_process(int ta_id, shared_data *data, int num_tas) {
    printf("TA %d started marking\n", ta_id);
    
    while (1) {
        // Check if we've reached the end
        if (data->current_exam > MAX_EXAMS) {
            printf("TA %d: No more exams to mark\n", ta_id);
            break;
        }
        
        // Load current exam info
        int current_exam = data->current_exam;
        printf("TA %d: Working on exam %d (%s)\n", ta_id, current_exam, data->exam_content);
        
        // Check if this is the termination exam
        if (strstr(data->exam_content, "9999") != NULL) {
            printf("TA %d: Reached termination exam, stopping\n", ta_id);
            data->current_exam = MAX_EXAMS + 1; // Signal other TAs to stop
            break;
        }
        
        // Review and potentially correct rubric
        printf("TA %d: Reviewing rubric\n", ta_id);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            // Random delay 0.5-1.0 seconds
            usleep(500000 + (rand() % 500000));
            
            // Random decision to correct (20% chance)
            if (rand() % 5 == 0) {
                printf("TA %d: Correcting rubric for question %d\n", ta_id, i+1);
                
                // Find the comma and modify the character after it
                char *comma = strchr(data->rubric[i], ',');
                if (comma && *(comma+2) != 0) { // +2 to skip comma and space
                    *(comma+2) = *(comma+2) + 1; // Next ASCII character
                    printf("TA %d: Updated rubric: %s\n", ta_id, data->rubric[i]);
                    
                    // Save to file
                    FILE *rubric_file = fopen("rubric.txt", "w");
                    if (rubric_file) {
                        for (int j = 0; j < NUM_QUESTIONS; j++) {
                            fprintf(rubric_file, "%s\n", data->rubric[j]);
                        }
                        fclose(rubric_file);
                    }
                }
            }
        }
        
        // Mark questions
        printf("TA %d: Starting to mark questions\n", ta_id);
        int questions_marked = 0;
        
        while (questions_marked < NUM_QUESTIONS) {
            // Find an unmarked question
            int question_to_mark = -1;
            for (int i = 0; i < NUM_QUESTIONS; i++) {
                if (data->questions_marked[i] == 0) {
                    question_to_mark = i;
                    break;
                }
            }
            
            if (question_to_mark == -1) {
                break; // All questions marked
            }
            
            // Mark the question (1.0-2.0 seconds)
            printf("TA %d: Marking question %d for exam %d\n", ta_id, question_to_mark+1, current_exam);
            usleep(1000000 + (rand() % 1000000));
            
            data->questions_marked[question_to_mark] = 1;
            questions_marked++;
            
            printf("TA %d: Marked question %d for student %s\n", ta_id, question_to_mark+1, data->exam_content);
        }
        
        // Move to next exam
        if (ta_id == 0) { // Let TA 0 handle loading next exam
            data->current_exam++;
            if (data->current_exam <= MAX_EXAMS) {
                load_exam(data, data->current_exam);
                printf("TA %d: Loaded next exam %d\n", ta_id, data->current_exam);
            }
        }
        
        // Small delay before next iteration
        usleep(100000);
    }
    
    printf("TA %d finished work\n", ta_id);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <number_of_TAs>\n", argv[0]);
        return 1;
    }
    
    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        printf("Number of TAs must be at least 2\n");
        return 1;
    }
    
    srand(time(NULL));
    
    // Create shared memory
    key_t key = ftok("shmfile", 65);
    int shmid = shmget(key, sizeof(shared_data), 0666|IPC_CREAT);
    shared_data *data = (shared_data*) shmat(shmid, NULL, 0);
    
    // Initialize shared data
    data->current_exam = 1;
    load_rubric(data);
    load_exam(data, 1);
    
    printf("Initial rubric loaded:\n");
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        printf("  %s\n", data->rubric[i]);
    }
    
    // Create TA processes
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process (TA)
            ta_process(i, data, num_tas);
            exit(0);
        } else if (pid < 0) {
            printf("Failed to create process for TA %d\n", i);
        }
    }
    
    // Wait for all child processes
    for (int i = 0; i < num_tas; i++) {
        wait(NULL);
    }
    
    // Cleanup shared memory
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);
    
    printf("All TAs finished marking\n");
    return 0;
}