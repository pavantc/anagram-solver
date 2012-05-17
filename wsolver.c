#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tree.h"

#define ASSERT(cond) {if (!(cond)) (*((char *)0) = 0);}

#define WORD_DB "/usr/share/dict/words"
#define MAX_WORD_SIZE 80

struct list {
	char *word;
	struct list *next;
};

struct word_node {
	char *word;
	RB_ENTRY(word_node) rb_node;
};

typedef struct word_node wnode_t;

typedef struct tree_handle {
	RB_HEAD(word_tree, word_node) th_tree;
} tree_handle_t;

/* Globals */
char copy[MAX_WORD_SIZE];
tree_handle_t th;
struct list *printed_wlist_head = NULL;

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


wnode_t *get_tree_node(char *str)
{
	wnode_t *w = ((wnode_t *) malloc(sizeof(wnode_t)));
	if (w) {
		w->word = malloc(strlen(str));
		if (w->word) {
			strcpy(w->word, str);
		} else {
			perror("malloc");
			exit(1);
		}
	} else {
		perror("malloc");
		exit(1);
	}
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

	node = get_tree_node(add_str);

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
	printf("Enter jumbled word: ");
	fflush(stdin);
	fgets(temp, MAX_WORD_SIZE, stdin);
	/* fgets() reads the newline into the buffer. Remove if present */
	for (i = 0; i < strlen(temp); i++) {
		if (temp[i] == '\n') {
			temp[i] = '\0';
		}
	}
}

void __attribute__((always_inline))
swap(char *a, char *b)
{
	int t;

	t = *a;
	*a = *b;
	*b = t;
}

/*
 * The recursive algorithm below does the following:
 * Lets take the letter combination - abc
 *
 * It starts off by producing all possible permutations starting with 'a'.
 * This is done by keeping 'a' unchanged and calling get_all_permutations
 * (recursively) with an advanced pointer (p+1). Note that the changes are being
 * done in-place and hence the input string is getting modified with each swap.
 * So, at the end of all iterations, get_all_permutations() makes sure that it
 * returns the input string to its initial state.
 *
 * In the above example, the following is the call stack():
 * We use the short form g_a_c() to represent get_all_permutations().
 * g_a_c(abc, 3)
 *   g_a_c(bc, 2)
 *     g_a_c(c, 1) -> Print 'abc'
 *   i is 0 here, which is less than (len - 1) => (2 - 1)
 *   swap elem0 with elem1 => b with c. String becomes cb
 *   g_a_c(cb, 2)
 *     g_a_c(b, 1) -> Print 'acb'
 *   i is 1 here which is NOT less than (2 - 1). Time to restore the string to
 *   its original position
 *   Rotate the current string to the left once to get back to the initial
 *   state.
 *   String no becomes 'bc'.
 *
 * After all the permutations starting with 'a', it brings the next letter in
 * the input string to the 0th place by swapping it with what was there
 * already. In this case, 'b' is swapped with 'a'. Now, the string becomes
 * 'bac'. It agains gets all permutations starting with 'b'. This way, it goes
 * all the way until all the for the last letter is obtained.
 */

struct list *
get_wlist_node(char *str)
{
	struct list *temp = malloc(sizeof(struct list));
	if (temp) {
		temp->word = malloc(strlen(str));
		if (temp->word) {
			strcpy(temp->word, str);
			temp->next = NULL;
		} else {
			perror("malloc");
			exit(1);
		}
	} else {
		perror("malloc");
		exit(1);
	}
}

int
search_printed_words(char *str)
{
	struct list *prev, *temp;

	if (printed_wlist_head == NULL) {
		printed_wlist_head = get_wlist_node(str);
		return (0);
	}

	for (prev = temp = printed_wlist_head; temp; prev = temp,
	    temp = temp->next) {
		if (strcmp(temp->word, str) == 0) {
			/* Found */
			return EEXIST;
		}
	}

	/* New word. Add to list */
	temp = get_wlist_node(str);
	prev->next = temp;
	return (0);
}

void
get_all_permutations(char *p, int len)
{
	int i, t;

	if (len == 1) {
		if (search_word_in_tree(&th, copy) == 0) {
			if (search_printed_words(copy) == 0) {
				printf("%s\n", copy);
			}
		}
		return;
	}
	for (i = 0; i < len; i++) {
		get_all_permutations(p+1, len - 1);
		/*
		 * Bring the next letter to position 0. get_all_permutations()
		 * starting with that letter.
		 */
		if (i < len - 1) {
			swap(&p[0], &p[i+1]);
		} else {
			/*
			 * Indicates completion of all permutations. Time to
			 * restore the string to its initial state. If the
			 * initial string was "abcde", at this point, due to 4
			 * swaps, (not 5, since we are in the fifth iteration)
			 * the string would be - "eabcd". We restore initial
			 * condition by doing a rotate_left_once.
			 */
			for (i = 0; i < len - 1; i++) {
				swap(&p[i], &p[i+1]);
			}
		}
	}
}

int
main()
{
	int ret;
	char temp[MAX_WORD_SIZE];

	init_tree(&th);
	populate_tree(&th);

	while(1) {
		query_word_from_user(temp);
		/* Using strcpy since the input is sanitized via fgets */
		strcpy(copy, temp);
		get_all_permutations(&copy[0], strlen(copy));
	}
	return (0);
}
