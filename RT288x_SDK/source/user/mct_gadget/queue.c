/*
 * Thread-Safe Queue
 *
 * queue.h
 *
 * A Thread-Safe FIFO queue implementation using double linked lists
 */

#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "mct_gadget.h"

void free_video_frame(PVIDEO_FRAME frame)
{
	if (frame == NULL)
		return;

	if (frame->buf != NULL){
		free(frame->buf);
		frame->buf = NULL;
	}

	free(frame);
}

VIDEO_FRAME * allocate_video_frame(int length)
{
	PVIDEO_FRAME frame = (PVIDEO_FRAME)malloc(sizeof(PVIDEO_FRAME));
	if(frame == NULL)
		return NULL;

	frame->buf = (char*)malloc(length);
	if(frame->buf == NULL) {
		free(frame);
		return NULL;
	}
	frame->length = length;
	return frame;
}

queue_t* queue_create()
{
	queue_t *queue = NULL;

	PRINT_MSG("%s \n",__func__);

	queue = malloc(sizeof(queue_t));
	if(queue == NULL)
		return NULL;

	queue->first = NULL;
	queue->last = NULL;
	queue->length = 0;
	if(queue != NULL)
		pthread_mutex_init(&queue->mutex, NULL);
	return queue;
}

void releses_queue(queue_t* queue)
{
 	PRINT_MSG("%s \n",__func__);

	if(queue == NULL)
		return;

	VIDEO_FRAME * frame = NULL;
	do {
		pthread_mutex_lock(&queue->mutex);
		PRINT_MSG("[queue_destroy] The queue is not empty=%d\n",queue->length);
		if (queue->length > 0) {
			pthread_mutex_unlock(&queue->mutex);
			frame = queue_remove(queue);
			if(frame != NULL)	{
				free_video_frame(frame);
			}
			continue;
		} else {
			pthread_mutex_unlock(&queue->mutex);
			break;
		}
	} while(1);
}

void queue_destroy(queue_t* queue)
{
	PRINT_MSG("%s \n",__func__);

	if (queue == NULL) {
		PRINT_MSG("[queue_destroy] Tried to destroy a 'NULL' queue\n");
		return;
	}

	pthread_mutex_destroy(&queue->mutex);
	free(queue);
	queue = NULL;

	PRINT_MSG("queue_destroy \n");
}

void queue_add(queue_t* queue, void* element)
{
	queue_element_t* new_element;

	DBG_MSG("%s \n",__func__);

	if (queue == NULL || element == NULL) {
		DBG_MSG("[queue_add] Invalid queue or element\n");
		return;
	}

	pthread_mutex_lock(&queue->mutex);
	new_element = malloc(sizeof(queue_element_t));
	if(new_element ==  NULL){
		pthread_mutex_unlock(&queue->mutex);
		return;
	}

	new_element->element = element;
	new_element->next = NULL;
	if (queue->first == NULL) {
		queue->first = new_element;
		queue->last = new_element;
	}
	else {
		queue->last->next = new_element;
		queue->last = new_element;
	}

	queue->length++;
	pthread_mutex_unlock(&queue->mutex);
}

int queue_length(queue_t* queue)
{
	if (queue) {
		DBG_MSG("%s, length = %d\n",__func__, queue->length);
		return queue->length;
	}
	return 0;
}

void * queue_first(queue_t *queue)
{
	pthread_mutex_lock(&queue->mutex);
	if ((queue == NULL) || (queue->first == NULL)) {
		pthread_mutex_unlock(&queue->mutex);
		return NULL;
	}

	pthread_mutex_unlock(&queue->mutex);
	return queue->first->element;
}

void * queue_remove(queue_t* queue)
{
	queue_element_t* node;
	pthread_mutex_lock(&queue->mutex);
	if ((queue == NULL) || (queue->first == NULL)) {
		pthread_mutex_unlock(&queue->mutex);
		return NULL;
	}
	node = queue->first;
	if (queue->first == queue->last) {
		queue->first = NULL;
		queue->last = NULL;
	} else {
		queue->first = node->next;
	}
	queue->length--;
	void* element = node->element;
	DBG_MSG("%s \n",__func__);
	free(node);
	pthread_mutex_unlock(&queue->mutex);
	return element;
}

