/**
 * ALFS - Anushiravan-Level Fair Scheduler
 * Main Entry Point
 * 
 * Usage: ./alfs_scheduler [options]
 *   -s, --socket <path>   Socket file path (default: event.socket)
 *   -c, --cpus <num>      Number of CPUs (default: 4)
 *   -q, --quanta <num>    Time quantum (default: 1)
 *   -m, --metadata        Include metadata in output
 *   -h, --help            Show help message
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include "alfs.h"
#include "scheduler.h"
#include "uds.h"
#include "json_handler.h"

/* Global flag for graceful shutdown */
static volatile int running = 1;

/* Command line options */
static struct option long_options[] = {
    {"socket",   required_argument, 0, 's'},
    {"cpus",     required_argument, 0, 'c'},
    {"quanta",   required_argument, 0, 'q'},
    {"metadata", no_argument,       0, 'm'},
    {"help",     no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/**
 * Print usage information
 */
static void print_usage(const char *program_name) {
    fprintf(stderr, "ALFS - Anushiravan-Level Fair Scheduler\n\n");
    fprintf(stderr, "Usage: %s [options]\n\n", program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -s, --socket <path>   Socket file path (default: %s)\n", DEFAULT_SOCKET_PATH);
    fprintf(stderr, "  -c, --cpus <num>      Number of CPUs (default: 4)\n");
    fprintf(stderr, "  -q, --quanta <num>    Time quantum (default: 1)\n");
    fprintf(stderr, "  -m, --metadata        Include metadata in output\n");
    fprintf(stderr, "  -h, --help            Show this help message\n");
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    /* Default configuration */
    char *socket_path = DEFAULT_SOCKET_PATH;
    int cpu_count = 4;
    int quanta = 1;
    bool include_metadata = false;
    
    /* Parse command line arguments */
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "s:c:q:mh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                socket_path = optarg;
                break;
            case 'c':
                cpu_count = atoi(optarg);
                if (cpu_count <= 0 || cpu_count > MAX_CPUS) {
                    fprintf(stderr, "Error: Invalid CPU count (must be 1-%d)\n", MAX_CPUS);
                    return 1;
                }
                break;
            case 'q':
                quanta = atoi(optarg);
                if (quanta <= 0) {
                    fprintf(stderr, "Error: Invalid quanta (must be > 0)\n");
                    return 1;
                }
                break;
            case 'm':
                include_metadata = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Print configuration */
    fprintf(stderr, "ALFS Scheduler Starting...\n");
    fprintf(stderr, "  Socket: %s\n", socket_path);
    fprintf(stderr, "  CPUs: %d\n", cpu_count);
    fprintf(stderr, "  Quanta: %d\n", quanta);
    fprintf(stderr, "  Metadata: %s\n", include_metadata ? "enabled" : "disabled");
    
    /* Initialize scheduler */
    Scheduler *sched = scheduler_init(cpu_count, quanta);
    if (!sched) {
        fprintf(stderr, "Error: Failed to initialize scheduler\n");
        return 1;
    }
    
    /* Connect to UDS */
    fprintf(stderr, "Connecting to socket: %s\n", socket_path);
    int sock = uds_connect(socket_path);
    if (sock < 0) {
        fprintf(stderr, "Error: Failed to connect to socket\n");
        scheduler_destroy(sched);
        return 1;
    }
    
    fprintf(stderr, "Connected. Waiting for events...\n");
    
    /* Main event loop */
    while (running) {
        /* Receive TimeFrame from tester */
        char *input = uds_receive_message(sock);
        if (!input) {
            if (errno == 0) {
                fprintf(stderr, "Connection closed by peer\n");
            } else {
                fprintf(stderr, "Error receiving message: %s\n", strerror(errno));
            }
            break;
        }
        
        /* Parse TimeFrame */
        TimeFrame *tf = json_parse_timeframe(input);
        if (!tf) {
            fprintf(stderr, "Error: Failed to parse TimeFrame\n");
            free(input);
            continue;
        }
        
        /* Process all events in this TimeFrame */
        for (int i = 0; i < tf->event_count; i++) {
            if (scheduler_process_event(sched, tf->events[i]) < 0) {
                fprintf(stderr, "Warning: Failed to process event %d at vtime %d\n", 
                        i, tf->vtime);
            }
        }
        
        /* Run scheduler for this tick */
        SchedulerTick *tick = scheduler_tick(sched, tf->vtime);
        if (!tick) {
            fprintf(stderr, "Error: Failed to generate scheduler tick\n");
            json_free_timeframe(tf);
            free(input);
            continue;
        }
        
        /* Serialize and send response */
        char *output = json_serialize_tick(tick, include_metadata);
        if (output) {
            if (uds_send_message(sock, output) < 0) {
                fprintf(stderr, "Error: Failed to send response\n");
            }
            free(output);
        } else {
            fprintf(stderr, "Error: Failed to serialize scheduler tick\n");
        }
        
        /* Cleanup */
        scheduler_tick_free(tick);
        json_free_timeframe(tf);
        free(input);
    }
    
    /* Cleanup */
    fprintf(stderr, "\nShutting down...\n");
    uds_disconnect(sock);
    scheduler_destroy(sched);
    
    fprintf(stderr, "ALFS Scheduler terminated.\n");
    return 0;
}
