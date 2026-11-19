/********* timing_test ********
 *
 * Purpose: Tries to find some key parameters regarding the system's timing abilities.
 *
 * By: Lumen M
 * Date: 11/18/2026
 *
 *************************/
#include <stdio.h>  
#include <time.h>  
 
#define SEC2NANOSEC 1000000000
#define NUMLOOPS 20

int main(void) {  
    struct timespec start, end, ts;  

    /**** CLOCK RESOLUTION ****/
    // Taken from stackoverflow, this is a clock resolution test.
    // https://stackoverflow.com/questions/18343188/precise-linux-timing-what-determines-the-resolution-of-clock-gettime 
    printf("=== Finding Resolution of Clock ===\n\n");
    ts.tv_sec  = 0; // s
    ts.tv_nsec = 1; // ns
    int iters = NUMLOOPS;
    double resTime, sum, max_val, min_val = 0;    
    for (int i = 0; i < iters; i++)
    {
        clock_gettime(CLOCK_MONOTONIC, &start);      // start timer
        clock_gettime(CLOCK_MONOTONIC, &end);        // end timer

        resTime = ((double)end.tv_sec*SEC2NANOSEC + (double)end.tv_nsec) 
                  - ((double)start.tv_sec*SEC2NANOSEC + (double)start.tv_nsec);
        sum = sum + resTime;
        printf("  Trial %d Resolution: %f ns\n", i, resTime);

        if (resTime > max_val) {
            max_val = resTime;
        }
        if (i == 0) {
            min_val = resTime;
        }
        if (resTime < min_val && resTime > 0) {
            min_val = resTime;
        }
    }    
    double res = sum / (double) iters;
    printf("Avg Clock Resolution:           %f\n", res);
    printf("Highest Clock Resolution:       %f\n", min_val);
    printf("Lowest Clock Resolution:        %f\n", max_val);
    
    /**** FUNCTION OVERHEAD ****/
    printf("=== Measuring Function Overhead ===\n\n");
    double overhead_sum = 0;
    double func_worst = 0;
    for (int i = 0; i < NUMLOOPS; i++) {
        // Start timer (monotonic clock, immune to manual clock jumps)  
        clock_gettime(CLOCK_MONOTONIC, &start);  
     
        // Call this function once
        clock_gettime(CLOCK_MONOTONIC, &ts);
     
        // Stop timer  
        clock_gettime(CLOCK_MONOTONIC, &end);  
     
        double time = ((double)end.tv_sec*SEC2NANOSEC + (double)end.tv_nsec) 
                    - ((double)start.tv_sec*SEC2NANOSEC + (double)start.tv_nsec);
     
        // I usually get 160-200ns on this, which quantifies the overhead of the function
        // without taking into account the fact that the OS's clock has a resolution of
        // 40ns. Thus we can be confident that this function will run in about 4-5 ticks (160-200ns). 
        // A good baseline then is to assume our measurements are only accurate to about 200ns, or
        // 0.2 micro seconds.
        printf("  Trial %d clock_gettime(): %.6f ns\n", i, time);
        
        overhead_sum += time;
        if (time > func_worst) {
            func_worst = time;
        }
    }
    double func_overhead = overhead_sum / NUMLOOPS;

    printf("Avg clock_gettime() Overhead:   %f\n", func_overhead);
    printf("Worst-Case clock_gettime() Overhead: %f\n", func_worst);

    /**** VERIFYING THE STUFF ABOVE ****/
    
    // Our clock is supposed to run at 1.3 GHz. That being said, when we run the code for 
    // getting clock resolution below, we tend to see measurements of 40ns or 80ns.
    // This means that our OS is ticking its clock at a rate of 25MHz, which is wayyy
    // slower than how fast the CPU is running. This loop is going to use our 0.2us 
    // expectation to see if we can measure something that takes 0.2, 0.4, 0.6, microseconds
    // up until 1 microsecond.
    printf("=== Running some loops ===\n\n");
    double prev = 0;
    double loop_sum = 0;

    volatile int i = 0;
    for (int loops = 0; loops < NUMLOOPS; loops++) {
        // 1.3 GHz means 0.77 nanoseconds per cycle
        // 0.2 microseconds / 0.77 nanoseconds --> 260 cycles
        // I dumped the assembly. It takes about 6 cycles for one loop (See bottom of this file)
        // 260 / 6 = 43.3 cycles per loop
        // Let's round to 50 as a starting point and then add 50 on each loop.
        // This should go from roughly 0.2 microseconds to 0.4 microseconds .... etc.
        
        // TODO: This doesn't work. We actually start at around 0.4 microseconds and increase... why?
        // Well, if we look at the assembly you can note that we aren't storing int i into a register.
        // I'm pretty sure this is being compiled without any optimization, so int i is just on the stack
        // which is definitely way slower than putting the variable into a register. A good next step
        // would be to figure out how to optimize the assembly to NOT use the stack, which would hopefully
        // remove overhead in the test.
        int num_loops = 50 + (50 * loops);
        // Start timer (monotonic clock, immune to manual clock jumps)  
        clock_gettime(CLOCK_MONOTONIC, &start);  
     
        for(i = 0; i < num_loops; i++);
     
        // Stop timer  
        clock_gettime(CLOCK_MONOTONIC, &end);  
     
        double time = ((double)end.tv_sec*SEC2NANOSEC + (double)end.tv_nsec) 
                    - ((double)start.tv_sec*SEC2NANOSEC + (double)start.tv_nsec);
     
        // If the result here is zero, that means our clock_gettime() function is so fast
        // that the board's timing clock does not have the resolution to actually measure
        // how long it takes to run this function.
        printf("  Trial for %d loops: %.6f ns\n", num_loops, time);

        // See how much each loop increases the time on avg
        loop_sum = loop_sum + (time - prev);
        prev = time;
    }
    printf("Avg Loop Increase:              %f\n\n", loop_sum / NUMLOOPS);

    printf("===== KEY RESULTS =====\n");
    printf("Clock Resolution:                    %f ns\n", min_val);
    printf("Worst-Case clock_gettime() Overhead: %f ns\n", func_worst);

    return 0;  
}

/*
Aha! Dumping the assembly for the loop shows us:

 8ec:	97ffff71 	bl	6b0 <clock_gettime@plt> // before loop
 8f0:	b90037ff 	str	wzr, [sp, #52]
 8f4:	14000004 	b	904 <main+0xdc>
 8f8:	b94037e0 	ldr	w0, [sp, #52]           // jumps here if cmp fails, START OF LOOP
 8fc:	11000400 	add	w0, w0, #0x1
 900:	b90037e0 	str	w0, [sp, #52]
 904:	b94037e0 	ldr	w0, [sp, #52]
 908:	b9406fe1 	ldr	w1, [sp, #108]
 90c:	6b00003f 	cmp	w1, w0                  // comparison! END OF LOOP
 910:	54ffff4c 	b.gt	8f8 <main+0xd0>
 914:	910123e0 	add	x0, sp, #0x48
 918:	aa0003e1 	mov	x1, x0
 91c:	52800020 	mov	w0, #0x1                   	
 920:	97ffff64 	bl	6b0 <clock_gettime@plt> // after loop


*/