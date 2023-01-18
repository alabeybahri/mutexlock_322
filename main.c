#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct customer_info{
    int customer_id;
    int sleep_time;
    int vending_machine_id;
    char* vending_company_name;
    int payment_amount;
};
struct vending_info{
    FILE *fp;
    int vending_id;
    int total_customer_number;
};
#define VENDING_MACHINE_NUMBER 10
#define TOTAL_COMPANY_NUMBER 5
pthread_mutex_t mutexQueue[VENDING_MACHINE_NUMBER];
pthread_cond_t condQueue[VENDING_MACHINE_NUMBER];
struct customer_info taskQueue[VENDING_MACHINE_NUMBER][300];
int task_count[VENDING_MACHINE_NUMBER] = {0,0,0,0,0,0,0,0,0,0};
int total_balances_of_companies[TOTAL_COMPANY_NUMBER] = {0,0,0,0,0};
int total_sales_on_companies[TOTAL_COMPANY_NUMBER] = {0,0,0,0,0};
pthread_mutex_t mutexBalances[TOTAL_COMPANY_NUMBER];
pthread_cond_t condBalances[TOTAL_COMPANY_NUMBER];

void customer_func(void* args){
    // Cast the given void pointer argument back into the customer_info struct
    struct customer_info customerInformation;
    customerInformation = *(struct customer_info *) args;
    // Sleep with the given amount of time
    usleep(customerInformation.sleep_time * 1000);
    // Lock the queue of the vending machine
    pthread_mutex_lock(&mutexQueue[customerInformation.vending_machine_id-1]);
    // Add customer information struct into the queue of the vending machine
    taskQueue[customerInformation.vending_machine_id-1][task_count[customerInformation.vending_machine_id-1]] = customerInformation;
    // Increase the task count of the corresponding vending machine
    task_count[customerInformation.vending_machine_id-1]++;
    // Unlock the vending machine queue
    pthread_mutex_unlock(&mutexQueue[customerInformation.vending_machine_id-1]);
    // Send signal to the waiting thread - Since signal already implements FIFO order, I've used it directly
    pthread_cond_signal(&condQueue[customerInformation.vending_machine_id-1]);
    pthread_exit(0);
}

void sell_ticket(struct customer_info customerInformation,int thread_id,FILE* fp){
    // Using the file pointer given print the selling information
    fprintf(fp,"[VTM%d]: Customer%d,%dTL,%s\n",thread_id,customerInformation.customer_id + 1,customerInformation.payment_amount,customerInformation.vending_company_name);
    // Since each company has its balances separately, I've added if cases to lock corresponding company balance
    // Also, instead of holding one variable for total sales globally, I've implemented it in an array to ensure program works parallel, otherwise only one sale can be made at a time
    if(strcmp(customerInformation.vending_company_name,"Kevin") == 0){
        pthread_mutex_lock(&mutexBalances[0]);
        total_balances_of_companies[0] += customerInformation.payment_amount;
        total_sales_on_companies[0] += 1;
        pthread_mutex_unlock(&mutexBalances[0]);
        pthread_cond_signal(&condBalances[0]);
    }
    if(strcmp(customerInformation.vending_company_name,"Bob") == 0){
        pthread_mutex_lock(&mutexBalances[1]);
        total_balances_of_companies[1] += customerInformation.payment_amount;
        total_sales_on_companies[1] += 1;
        pthread_mutex_unlock(&mutexBalances[1]);
        pthread_cond_signal(&condBalances[1]);
    }
    if(strcmp(customerInformation.vending_company_name,"Stuart") == 0){
        pthread_mutex_lock(&mutexBalances[2]);
        total_balances_of_companies[2] += customerInformation.payment_amount;
        total_sales_on_companies[2] += 1;
        pthread_mutex_unlock(&mutexBalances[2]);
        pthread_cond_signal(&condBalances[2]);
    }
    if(strcmp(customerInformation.vending_company_name,"Otto") == 0){
        pthread_mutex_lock(&mutexBalances[3]);
        total_balances_of_companies[3] += customerInformation.payment_amount;
        total_sales_on_companies[3] += 1;
        pthread_mutex_unlock(&mutexBalances[3]);
        pthread_cond_signal(&condBalances[3]);
    }
    if(strcmp(customerInformation.vending_company_name,"Dave") == 0){
        pthread_mutex_lock(&mutexBalances[4]);
        total_balances_of_companies[4] += customerInformation.payment_amount;
        total_sales_on_companies[4] += 1;
        pthread_mutex_unlock(&mutexBalances[4]);
        pthread_cond_signal(&condBalances[4]);
    }
}

void vending_machine_func(void* args) {
    // Cast the given void pointer argument back into the vending_info struct
    struct vending_info vendingInformation;
    vendingInformation = *(struct vending_info *) args;
    int vending_machine_id = vendingInformation.vending_id;
    while (1) {
        struct customer_info customerInfo;
        // Lock the mutex queue
        pthread_mutex_lock(&mutexQueue[vending_machine_id - 1]);
        while (task_count[vending_machine_id-1] < 1) {
            // If there is no task, wait. Also unlock the queue lock so customers can be able to lock
            pthread_cond_wait(&condQueue[vending_machine_id - 1], &mutexQueue[vending_machine_id - 1]);
        }
        // If any customer waiting, get that customer information from the array
        customerInfo = taskQueue[vending_machine_id - 1][0];
        int i;
        // After that, shift all the elements in the queue to left by one index and decrease corresponding task count number
        for (i = 0; i < task_count[vending_machine_id-1]; i++) {
            taskQueue[vending_machine_id - 1][i] = taskQueue[vending_machine_id - 1][i + 1];
        }
        task_count[vending_machine_id-1]--;
        // Unlock the queue lock, after unlocking send customer into the sell ticket method, if we call method before unlocking the other threads won't be able to lock queue
        pthread_mutex_unlock(&mutexQueue[vending_machine_id - 1]);
        sell_ticket(customerInfo,vending_machine_id,vendingInformation.fp);
        // Check for total sales are equal to customers, if so exit from thread
        int total_sales = 0;
        for (int j = 0; j < TOTAL_COMPANY_NUMBER; ++j) {
            // I've added lock here because we're reading a value from memory which other threads may write on
            pthread_mutex_lock(&mutexBalances[j]);
            total_sales += total_sales_on_companies[j];
            pthread_mutex_unlock(&mutexBalances[j]);
            pthread_cond_signal(&condBalances[j]);
        }
        if(total_sales >= vendingInformation.total_customer_number){
            pthread_exit(0);
        }
    }
}


int main(int argc, char **argv)
{
    FILE *fpout;
    // Get the name of the input file from argument
    char* inputFile = argv[1];
    // Read from the input file
    FILE * fp; char * line = NULL; size_t len = 0; char* strtol_ptr;
    fp = fopen(inputFile, "r");
    // Create output file
    int strLen = (int)strlen(inputFile) - 4;
    inputFile[strLen] = '\0';
    strcat(inputFile,"_log.txt");
    char *outputFile = inputFile;
    fpout = fopen(outputFile, "w");
    // Initialize the mutexes and cond
    for (int i = 0; i < VENDING_MACHINE_NUMBER; ++i) {
        pthread_cond_init(&condQueue[i],NULL);
        pthread_mutex_init(&mutexQueue[i],NULL);
    }
    for (int i = 0; i < TOTAL_COMPANY_NUMBER; ++i) {
        pthread_cond_init(&condBalances[i],NULL);
        pthread_mutex_init(&mutexBalances[i],NULL);
    }

    if (fp == NULL)
        exit(EXIT_FAILURE);
    getline(&line,&len,fp);
    // Cast the number of customers to int
    int NUMBER_OF_CUSTOMERS = (int)strtol(line, &strtol_ptr,10);
    // Create array of structs to hold data of vending machines and customers
    struct customer_info customerInfo[NUMBER_OF_CUSTOMERS];
    struct vending_info vendingInfo[VENDING_MACHINE_NUMBER];
    // Create thread id array for customer threads and vending machine threads
    pthread_t customer_tid[NUMBER_OF_CUSTOMERS];
    pthread_t vending_machine_tid[VENDING_MACHINE_NUMBER];
    // Create the vending machine threads
    for (int i = 0; i < VENDING_MACHINE_NUMBER; ++i) {
        vendingInfo[i].total_customer_number = NUMBER_OF_CUSTOMERS;
        vendingInfo[i].vending_id = i + 1;
        vendingInfo[i].fp = fpout;
        pthread_create(&vending_machine_tid[i], NULL, (void *(*)(void *)) vending_machine_func, &vendingInfo[i]);
    }
    // Read from the file values of customers
    for(int i=0;i<NUMBER_OF_CUSTOMERS;i++){
        getline(&line, &len, fp);
        char* sleep_time = strsep(&line, ",");
        int sleep_time_int = (int)strtol(sleep_time,&strtol_ptr,10);
        char* vending_machine_id = strsep(&line, ",");
        int vending_machine_id_int = (int)strtol(vending_machine_id,&strtol_ptr,10);
        char* company_name = strsep(&line, ",");
        char* payment_amount = strsep(&line, ",");
        int payment_amount_int = (int)strtol(payment_amount,&strtol_ptr,10);
        // Edit struct array of customer
        customerInfo[i].sleep_time = sleep_time_int;
        customerInfo[i].vending_machine_id = vending_machine_id_int;
        customerInfo[i].vending_company_name = company_name;
        customerInfo[i].payment_amount = payment_amount_int;
        customerInfo[i].customer_id = i;
        // Create customer thread with the argument of it's information
        pthread_create(&customer_tid[i], NULL, (void *(*)(void *)) customer_func, &customerInfo[i]);
    }
    // Join customer threads to NULL
    for (int i = 0; i < NUMBER_OF_CUSTOMERS; ++i) {
        pthread_join(customer_tid[i],NULL);
    }
    // Destroy mutexes and cond
    for (int i = 0; i < VENDING_MACHINE_NUMBER; ++i) {
        pthread_cond_destroy(&condQueue[i]);
        pthread_mutex_destroy(&mutexQueue[i]);
    }
    for (int i = 0; i < TOTAL_COMPANY_NUMBER; ++i) {
        pthread_cond_destroy(&condBalances[i]);
        pthread_mutex_destroy(&mutexBalances[i]);
    }
    fclose(fp);
    if (line)
        free(line);
    // Print values into the output file in the main thread
    fprintf(fpout, "[Main]: All payments are completed\n");
    fprintf(fpout, "[Main]: Kevin: %d\n",total_balances_of_companies[0]);
    fprintf(fpout, "[Main]: Bob: %d\n",total_balances_of_companies[1]);
    fprintf(fpout, "[Main]: Stuart: %d\n",total_balances_of_companies[2]);
    fprintf(fpout, "[Main]: Otto: %d\n",total_balances_of_companies[3]);
    fprintf(fpout, "[Main]: Dave: %d\n",total_balances_of_companies[4]);
    fclose(fpout);
    exit(EXIT_SUCCESS);
}
