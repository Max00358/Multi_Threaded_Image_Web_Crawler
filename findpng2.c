#include <curl/curl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include "http_search.h"
#include "url_vector.h"

#define URL_HEAD "http://ece252-1.uwaterloo.ca/lab4"
#define	URL_PNG "http://ece252-3.uwaterloo.ca:2540/image?q=gAAAAABdHkoqOKR-cFRCkiCUBEMAAAWfDvBFlRisL9ysLWHYHbcQbn1b28PV_uHBZ0gJf5bvzrnf1HNXxB6KRlAVETwTIqBH2Q=="
#define URL_ERR "http://ece252-2.uwaterloo.ca:2542/image?q=gAAAAABdHlHZmbxmnenlxhndlipqfntjijjammhualsfjfkbgruhpprimngcjssinigxalforazyxxmsbuhxcsqwstfveoonvi=="

void* thread_function(void* arg);
bool search_log_and_push_log(char* p_URL);
bool is_png(char* p_URL);

pthread_mutex_t frontier_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t png_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t wake_condition = PTHREAD_COND_INITIALIZER;

URL_VECTOR log_vec = { .size = 0 };
URL_VECTOR frontier_vec = { .size = 0 };
URL_VECTOR png_vec = { .size = 0 };

int* slept_arr;
pthread_t* threads;

unsigned slept_threads = 0;
unsigned thread_count = 0;
unsigned max_png_count = 0;
unsigned killed_threads = 0;

atomic_int png_counter = ATOMIC_VAR_INIT(0);

void print_slept_thread(void){
	printf("Slept threads: ");
	for(int i = 0; i < thread_count; i++){
		printf("%d - ", slept_arr[i]);
	}
	printf("\n");
}

int main(int argc, char** argv) {
	double times[2];
	struct timeval tv;
	if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
	times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);


	int c;
	int t = 1;
	int m = 50;
	char* p_v = NULL;
	char *str = "option requires an argument";

	bool log_v = false;
	int args = 1;
   
	while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
		switch (c) {
			case 't':
		    t = strtoul(optarg, NULL, 10);
			args += 2;
		    printf("option -t specifies a value of %d.\n", t);
			if (t <= 0) {
			    fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
				return -1;
	        }
		        break;
			case 'm':
				m = strtoul(optarg, NULL, 10);
				args += 2;
				printf("option -m specifies a value of %d.\n", m);
		        if (m <= 0) {
			        fprintf(stderr, "%s: %s 1, 2, or 3 -- 'm'\n", argv[0], str);
				    return -1;
	            }
		        break;
			case 'v':
				if (optind - 1 < argc) {
					p_v = argv[optind - 1];
					log_v = true;
					args += 2;
					printf("option -v specifies a value of %s.\n", p_v);
				}
			default:
				continue;
        }
    }
	thread_count = t;
	max_png_count = m;
	char* p_head = NULL;

	if (args < argc) {
		printf("URL: %s\n", argv[args]);
		p_head = (char*)malloc((strlen(argv[args]) + 1)*sizeof(char));
		p_head[strlen(argv[args])] = '\n';
		strcpy(p_head, argv[args]);
		printf("%s\n", p_head);
	} else {
		printf("DEFAULT\n");
		p_head = (char*)malloc((strlen(URL_HEAD) + 1)*sizeof(char));
		p_head[strlen(URL_HEAD)] = '\n';
		strcpy(p_head, URL_HEAD);
		printf("%s\n", p_head);
	}

	printf("Log File: %s\n", log_v ? p_v : "false");

	
	curl_global_init(CURL_GLOBAL_DEFAULT);
	xmlInitParser();
	
	threads = (pthread_t*)malloc(thread_count * sizeof(pthread_t));
	slept_arr = (int*)malloc(thread_count * sizeof(int));
	
	for(int i = 0; i < thread_count; i++){
		threads[i] = 0;
		slept_arr[i] = 0;
	}
	for (unsigned i = 0; i < MAX_SIZE; i++) {
		log_vec.urls[i] = NULL;
		frontier_vec.urls[i] = NULL;
	}

	// push head url
	//char* p_head = (char*)malloc((strlen(URL_HEAD) + 1)*sizeof(char));
	//strcpy(p_head, URL_HEAD);
	push(&frontier_vec, p_head); 
	
	// create and wait for threads
	for (unsigned i = 0; i < thread_count; i++) {
		pthread_create(threads + i, NULL, thread_function, NULL);
	}
	for (unsigned i = 0; i < thread_count; i++) {
		pthread_join(threads[i], NULL);
	}
	printf("PNGs: %d\n", png_vec.size);

	// Create Files
	FILE* p_png_urls = fopen("png_urls.txt", "w");
	if (!p_png_urls) { exit(1); }

	for (unsigned i = 0; i < png_vec.size; i++) {
		if (i == max_png_count) { break; }
		fprintf(p_png_urls, "%s\n", png_vec.urls[i]);
	}
	fclose(p_png_urls);

	if (log_v){
		FILE* p_log_file = fopen(p_v, "w");
		if (!p_log_file) { exit(1); }
		
		for (unsigned i = 0; i < log_vec.size; i++)
			fprintf(p_log_file, "%s\n", log_vec.urls[i]);
		fclose(p_log_file);
		printf("Logged to: %s\n", p_v);
	} else {
		printf("Did Not Log\n");
	}

	// cleanup
	printf("Log Vec Cleanup\n");
	for (unsigned i = 0; i < log_vec.size; i++) {
		if (log_vec.urls[i]) free(log_vec.urls[i]);
		log_vec.urls[i] = NULL;
	}
	printf("PNG Vec Cleanup\n");
	for (unsigned i = 0; i < png_vec.size; i++) {
		if (png_vec.urls[i]) free(png_vec.urls[i]);
		png_vec.urls[i] = NULL;
	}
	printf("Frontier Vec Cleanup\n");
	for (unsigned i = 0; i < frontier_vec.size; i++) {
		if (frontier_vec.urls[i]) free(frontier_vec.urls[i]);
		frontier_vec.urls[i] = NULL;	
	}

	free(threads);
	free(slept_arr);

	pthread_mutex_destroy(&frontier_mutex);
    pthread_mutex_destroy(&log_mutex);
    pthread_mutex_destroy(&png_mutex);
	pthread_cond_destroy(&wake_condition);
	
	xmlCleanupParser();
	curl_global_cleanup();
	
	clock_gettime(CLOCK_MONOTONIC, &end);
	double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
	printf("paster2 execution time: %.6lf seconds\n", elapsed_time);

	if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;

	return 0;
}

void* thread_function(void* arg) {
	long int n = 0;
	pthread_t tid = pthread_self();
	for(int i = 0; i < thread_count; i++){
		if(threads[i] == tid){
			n = i + 1;
		}
	}
	while (true) {
		char* p_URL = NULL;
		bool pushed_to_log = false;

		pthread_mutex_lock(&frontier_mutex);
			if (atomic_load(&png_counter) >= max_png_count) {
				goto __thread_exit;
			}
			while (frontier_vec.size == 0) {
				if (slept_threads == thread_count - 1) {
__thread_exit:
					++slept_threads;
					pthread_mutex_unlock(&frontier_mutex);
					pthread_cond_signal(&wake_condition);
					pthread_exit(NULL);
				} else {
					++slept_threads;
					pthread_cond_wait(&wake_condition, &frontier_mutex);
					--slept_threads;
				}
			}

			p_URL = pop(&frontier_vec);
		pthread_mutex_unlock(&frontier_mutex);
		
		pushed_to_log = search_log_and_push_log(p_URL);
		if (pushed_to_log) {
			search_web_and_push_frontier(p_URL, &png_vec, &frontier_vec,
										 &png_mutex, &frontier_mutex,
										 &wake_condition, n,
										 &png_counter, &max_png_count);
		}
		if (p_URL) {
			free(p_URL);
		}
	}

	pthread_exit(NULL);
}	
bool search_log_and_push_log(char* p_URL) {
	if (!p_URL) return false;
	
	bool found = false;
	bool pushed_to_log = false;
    char* p_new = NULL;
	int new_len = 0;

	new_len = strlen(p_URL);
	if (new_len == 0) { return pushed_to_log; }
	p_new = (char*)malloc((new_len + 1)*sizeof(char));
	p_new[new_len] = '\0';
	strcpy(p_new, p_URL);
	
	pthread_mutex_lock(&log_mutex);
		for (unsigned i = 0; i < log_vec.size; i++) {
			if (!strcmp(log_vec.urls[i], p_URL)) {
				found = true;
				break;
			}
		}
		if (!found) {
			pushed_to_log = push(&log_vec, p_new);
		}
	pthread_mutex_unlock(&log_mutex);
	if (found) {
		free(p_new);
	}
	
	return pushed_to_log;
}


