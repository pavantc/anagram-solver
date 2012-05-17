#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tree.h"

#define ASSERT(cond) {if (!(cond)) (*((char *)0) = 0);}

#define WORD_DB "/usr/share/dict/words"
#define MAX_WORD_SIZE 80

struct word_node {
	char *word;
	RB_ENTRY(word_node) rb_node;
};

typedef struct word_node wnode_t;

typedef struct tree_handle {
	RB_HEAD(word_tree, word_node) th_tree;
} tree_handle_t;

int
str_compare(const void *query_key, const void *cur)
{
	char *x = ((wnode_t *)query_key)->word;
	char *y = ((wnode_t *)cur)->word;

	if (strcmp(x, y) < 0) {
		return (-1);
	} else if (strcmp(x, y) > 0) {
		return (1);
	} else {
		return (0);
	}
}

RB_PROTOTYPE(word_tree, word_node, rb_node, str_compare);
RB_GENERATE(word_tree, word_node, rb_node, str_compare);


wnode_t *get_node(char *str)
{
	wnode_t *w = ((wnode_t *) malloc(sizeof(wnode_t)));
	w->word = malloc(strlen(str));
	strcpy(w->word, str);
	return (w);
}

void
init_tree(tree_handle_t *handle)
{
	RB_INIT(&handle->th_tree);
}

int
add_word_to_tree(tree_handle_t *handle, char *add_str)
{
	wnode_t *node;
	int ret = 0;

	ASSERT(add_str != NULL);

	node = get_node(add_str);

	if (RB_FIND(word_tree, &handle->th_tree, node) != NULL) {
		/* Node already present */
		free(node);
		ret = EEXIST;
	} else {
		RB_INSERT(word_tree, &handle->th_tree, (void *)node);
	}

	return (ret);
}

int
search_word_in_tree(tree_handle_t *handle, char *search_str)
{
	wnode_t temp, *node;
	int ret;

	memset((void *)&temp, 0, sizeof(wnode_t));
	temp.word = search_str;

	if ((node = RB_FIND(word_tree, &handle->th_tree, &temp)) != NULL) {
		/* Found */
		ret = 0;
	} else {
		/* Not Found */
		ret = ENOENT;
	}

	return (ret);
}

int
populate_tree(tree_handle_t *tree)
{
	/* Assumption : no word in the WORD_DB is >= MAX_WORD_SIZE characters long */
	FILE *fp;
	char temp[MAX_WORD_SIZE];

	fp = fopen(WORD_DB, "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open word database at : %s\n",
		    WORD_DB);
		exit(1);
	}

	while(fscanf(fp, "%s", temp) != EOF) {
		//printf("Adding %s to tree\n", temp);
		if (add_word_to_tree(tree, temp) == EEXIST) {
			fprintf(stderr, "%s already in tree\n", temp);
		}
	}

	fclose(fp);
	return (0);
}

int
query_word_from_user(char *temp)
{
	int i;
	printf("Enter word: ");
	fflush(stdin);
	fgets(temp, MAX_WORD_SIZE, stdin);
	/* fgets() reads the newline into the buffer. Remove if present */
	for (i = 0; i < strlen(temp); i++) {
		if (temp[i] == '\n') {
			temp[i] = '\0';
		}
	}
}

int
main()
{
	tree_handle_t th;
	int ret;
	char temp[MAX_WORD_SIZE];

	init_tree(&th);
	populate_tree(&th);

	while(1) {
		query_word_from_user(temp);
		ret = search_word_in_tree(&th, temp);
		if (ret == 0) {
			printf("%s found in tree\n", temp);
		} else {
			printf("%s not found in tree\n", temp);
		}
	}
	return (0);
}
