#pragma once


#ifndef PKT_QUEUE_H
#define PKT_QUEUE_H



struct queue_root {
	struct queue_node *head;
	struct queue_node *tail;
	volatile unsigned int size;
};

struct queue_node {
	void *n;
	struct queue_node *next;
};

void init_queue(struct queue_root **root);
int queue_add(struct queue_root *root, void *val);
void *queue_get(struct queue_root *root);

void free_queue(struct queue_root *root);

#endif /* !PKT_QUEUE_H */