#include "controlled.h"
#include "pkt_queue.h"



#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "pkt_queue.h"

#define compare_and_swap_pointer(Destination,Comperand,ExChange) \
 (InterlockedCompareExchangePointer(Destination, ExChange, Comperand) == Comperand)

//__forceinline int T_InterlockedCompareExchangePointer(PVOID volatile * Destination, PVOID ExChange, PVOID Comperand)
//{
//	// 源，交换值,比较值
//	return	InterlockedCompareExchangePointer(Destination, ExChange, Comperand) == Comperand;
//}

void free_queue(struct queue_root *root)
{
	void* item = NULL;
	while ((item = queue_get(root))!=NULL) {
		free(item);
	}

	free(root->head);
	free(root);
}

void init_queue(struct queue_root **root)
{
	*root = (struct queue_root*) malloc(sizeof(struct queue_root));
	if (root == NULL) {
		printf("malloc failed");
		exit(1);
	}
	(*root)->head = (struct queue_node*) malloc(sizeof(struct queue_node)); /* Sentinel node */
	(*root)->tail = (*root)->head;
	(*root)->head->n = NULL;
	(*root)->head->next = NULL;
}

int queue_add(struct queue_root *root, void *val)
{
	struct queue_node *n;
	struct queue_node *node = (struct queue_node *)malloc(sizeof(struct queue_node));
	node->n = val;
	node->next = NULL;
	while (1) {
		n = root->tail;
		if (compare_and_swap_pointer((void**)&(n->next), NULL, node)) {
			break;
		}
	}
	compare_and_swap_pointer((void**)&(root->tail), n, node);

	return 1;
}

void *queue_get(struct queue_root *root)
{
	struct queue_node *n;
	void *val;
	while (1) {
		n = root->head;
		if (n->next == NULL) {
			return NULL;
		}

		if (compare_and_swap_pointer((void**)&(root->head), n, n->next)) {
			break;
		}
	}
	val = (void *)n->next->n;
	free(n);
	return val;
}