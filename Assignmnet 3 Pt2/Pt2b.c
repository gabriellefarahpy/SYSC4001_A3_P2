#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define MAX_EXAMS 20
#define NUM_QUESTIONS 5
#define SHM_SIZE 1024

// Union for semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

typedef struct {
    char rubric[NUM_QUESTIONS][100];
    int current_exam;
    int questions_marked[NUM_QUESTIONS];
    char exam_content[100];
    int active_tas;
} shared_data;

// Semaphore indices
#define RUBRIC_SEM 0    // Controls access to rubric modification
#define EXAM_LOAD_SEM 1 // Controls loading next exam
#define QUESTION_SEM 2  // Base for question semaphores

void semaphore_wait(int semid, int sem_num) {
    struct sembuf sb = {sem_num, -1, 0};
    semop(semid, &sb, 1);
}

void semaphore_signal(int semid, int sem_num) {
    struct sembuf sb = {sem_num, 1, 0};
    semop(semid, &sb, 1);
}

void load_rubric(shared_data *data) {
    FILE *file = fopen("rubric.txt", "r");
    if (file) {
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            fgets(data->rubric[i], 100, file);
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
    
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        data->questions_marked[i] = 0;
    }
    
    data->current_exam = exam_num;
}

void ta_process(int ta_id, shared_data *data, int semid, int num_tas) {
    printf("TA %d started marking\n", ta_id);
    
    while (1) {
        // Check termination condition
        semaphore_wait(semid, EXAM_LOAD_SEM);
        if (data->current_exam > MAX_EXAMS) {
            semaphore_signal(semid, EXAM_LOAD_SEM);
            printf("TA %d: No more exams to mark\n", ta_id);
            break;
        }
        
        int current_exam = data->current_exam;
        char current_student[100];
        strcpy(current_student, data->exam_content);
        semaphore_signal(semid, EXAM_LOAD_SEM);
        
        // Check for termination exam
        if (strstr(current_student, "9999") != NULL) {
            semaphore_wait(semid, EXAM_LOAD_SEM);
            data->current_exam = MAX_EXAMS + 1;
            semaphore_signal(semid, EXAM_LOAD_SEM);
            printf("TA %d: Reached termination exam, stopping\n", ta_id);
            break;
        }
        
        printf("TA %d: Working on exam %d (%s)\n", ta_id, current_exam, current_student);
        
        // Review and potentially correct rubric
        printf("TA %d: Reviewing rubric\n", ta_id);
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            usleep(500000 + (rand() % 500000));
            
            if (rand() % 5 == 0) {
                // Acquire rubric semaphore for modification
                semaphore_wait(semid, RUBRIC_SEM);
                printf("TA %d: Correcting rubric for question %d\n", ta_id, i+1);
                
                char *comma = strchr(data->rubric[i], ',');
                if (comma && *(comma+2) != 0) {
                    *(comma+2) = *(comma+2) + 1;
                    printf("TA %d: Updated rubric: %s\n", ta_id, data->rubric[i]);
                    
                    FILE *rubric_file = fopen("rubric.txt", "w");
                    if (rubric_file) {
                        for (int j = 0; j < NUM_QUESTIONS; j++) {
                            fprintf(rubric_file, "%s\n", data->rubric[j]);
                        }
                        fclose(rubric_file);
                    }
                }
                semaphore_signal(semid, RUBRIC_SEM);
            }
        }
        
        // Mark questions
        printf("TA %d: Starting to mark questions\n", ta_id);
        int questions_marked = 0;
        
        while (questions_marked < NUM_QUESTIONS) {
            int question_to_mark = -1;
            
            // Find an unmarked question (with synchronization)
            for (int i = 0; i < NUM_QUESTIONS; i++) {
                semaphore_wait(semid, QUESTION_SEM + i);
                if (data->questions_marked[i] == 0) {
                    data->questions_marked[i] = ta_id + 1; // Mark as being marked by this TA
                    question_to_mark = i;
                    semaphore_signal(semid, QUESTION_SEM + i);
                    break;
                }
                semaphore_signal(semid, QUESTION_SEM + i);
            }
            
            if (question_to_mark == -1) {
                break; // All questions marked or being marked
            }
            
            printf("TA %d: Marking question %d for exam %d\n", ta_id, question_to_mark+1, current_exam);
            usleep(1000000 + (rand() % 1000000));
            
            printf("TA %d: Marked question %d for student %s\n", ta_id, question_to_mark+1, current_student);
            questions_marked++;
        }
        
        // Check if all questions are marked and load next exam
        semaphore_wait(semid, EXAM_LOAD_SEM);
        int all_marked = 1;
        for (int i = 0; i < NUM_QUESTIONS; i++) {
            if (data->questions_marked[i] == 0) {
                all_marked = 0;
                break;
            }
        }
        
        if (all_marked && data->current_exam == current_exam) {
            data->current_exam++;
            if (data->current_exam <= MAX_EXAMS) {
                load_exam(data, data->current_exam);
                printf("TA %d: Loaded next exam %d\n", ta_id, data->current_exam);
            }
        }
        semaphore_signal(semid, EXAM_LOAD_SEM);
        
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
    
    // Create semaphores
    int semid = semget(key, NUM_QUESTIONS + 3, 0666|IPC_CREAT);
    
    // Initialize semaphores
    union semun arg;
    unsigned short values[NUM_QUESTIONS + 3];
    
    values[RUBRIC_SEM] = 1;    // Binary semaphore for rubric access
    values[EXAM_LOAD_SEM] = 1; // Binary semaphore for exam loading
    for (int i = 0; i < NUM_QUESTIONS; i++) {
        values[QUESTION_SEM + i] = 1; // Binary semaphores for each question
    }
    
    arg.array = values;
    semctl(semid, 0, SETALL, arg);
    
    // Initialize shared data
    data->current_exam = 1;
    data->active_tas = num_tas;
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
            ta_process(i, data, semid, num_tas);
            exit(0);
        } else if (pid < 0) {
            printf("Failed to create process for TA %d\n", i);
        }
    }
    
    // Wait for all child processes
    for (int i = 0; i < num_tas; i++) {
        wait(NULL);
    }
    
    // Cleanup
    shmdt(data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    
    printf("All TAs finished marking\n");
    return 0;
}