#include "url_vector.h"

#ifndef HTTP_SEARCH_H
#define HTTP_SEARCH_H

int search_web_and_push_frontier(
 	char* p_URL,
	URL_VECTOR* p_png_vec,
	URL_VECTOR* p_frontier_vec,
	pthread_mutex_t* p_png_mutex,
	pthread_mutex_t* p_frontier_mutex,
	pthread_cond_t* p_wake_condition,
	long int n,
	atomic_int* p_png_counter,
	unsigned* p_max_png_count
);


#endif // HTTP_SEARCH_H

