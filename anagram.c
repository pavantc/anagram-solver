#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "tree.h"

#define ASSERT(cond) {if (!(cond)) (*((char *)0) = 0);}

#define WORD_DB "/usr/share/dict/words"
#define MAX_WORD_SIZE 80
#define PLACE_HOLDER_CHAR '9'

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
struct list *word_list_head = NULL;
int stack_top;
char *stack[MAX_WORD_SIZE];

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
add_to_word_list(char *str)
{
	struct list *prev, *temp;

	if (word_list_head == NULL) {
		word_list_head = get_wlist_node(str);
		return (0);
	}

	for (prev = temp = word_list_head; temp;
	    prev = temp, temp = temp->next) {
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

void
get_all_permutations(char *p, int len)
{
	int i, t;

	if (len == 1) {
		char *q;
		if (*p == 'a' || *p == 'i' || *p == 'A' || *p == 'I') {
			/* The only two single letter words */
			add_to_word_list(p);
		}
		for (q = copy; strlen(q) >= 2 && *q != '\0'; q++) {
			if (search_word_in_tree(&th, q) == 0) {
				add_to_word_list(q);
			}
		}
		return;
	}

	for (i = 0; i < len; i++) {

		/* check if p is a word in the dictionary */
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

/* Check if string "str" is a subset of "clist" */
int
word_in_charlist(char *clist, char *str)
{
	char *p, *q;
	char copy_clist[MAX_WORD_SIZE];
	int found;

	strcpy(copy_clist, clist);

	for (p = str; *p != '\0'; p++) {
		found = 0;
		for (q = &copy_clist[0]; *q != '\0'; q++) {
			if (*p == *q) {
				*q = PLACE_HOLDER_CHAR;
				found = 1;
				break;
			}
		}
		if (!found) {
			return (0);
		}
	}
	strcpy(clist, copy_clist);
	return (1);
}

void
add_to_charlist(char *clist, char *str)
{
	char *p, *q;

	q = str;

	for (p = clist; *p != '\0'; p++) {
		if (*p == PLACE_HOLDER_CHAR) {
			if (*q == '\0') {
				/* We are done. Return. */
				return;
			} else {
				*p = *q;
				q++;
			}
		}
	}
}

void
init_stack()
{
	int i;
	stack_top = -1;
	for (i = 0; i < MAX_WORD_SIZE; i++) {
		stack[i] = malloc(MAX_WORD_SIZE);
		if (!stack[i]) {
			perror("malloc");
			exit(1);
		}
	}
}

void
push(char *str)
{
	strcpy(stack[++stack_top], str);
}

void
pop()
{
	stack_top--;
}

void
print_stack()
{
	int i;

	for (i = 0; i <= stack_top; i++) {
		printf("%s%s", stack[i], (i == stack_top ? "" : " "));
	}
	printf("\n");
}

void
get_anagrams(struct list *head, int len, char *charlist)
{
	struct list *temp;
	int wlen;

	if (head == NULL) {
		return;
	}

	for (temp = head; len && temp; temp = temp->next) {
		wlen = strlen(temp->word);
		if (wlen <= len && word_in_charlist(charlist, temp->word)) {
			push(temp->word);
			len -= wlen;
			if (len) {
				get_anagrams(temp->next, len, charlist);
			}
			if (len == 0)
				print_stack();
			pop();
			add_to_charlist(charlist, temp->word);
			len += wlen;
		}
	}
}

void cleanup_lists()
{
	struct list *cur, *next;

	for (cur = next = word_list_head; cur; ) {
		next = cur->next;
		free(cur->word);
		free(cur);
		cur = next;
	}
	word_list_head = NULL;
}

/* O(n^2) algorithm assuming that the wordlist is not too big */
void
sort_word_list(struct list *wlist)
{
	struct list *t1, *t2;
	char *temp;

	for (t1 = wlist; t1; t1 = t1->next) {
		for (t2 = t1; t2; t2 = t2->next) {
			if (strcmp(t1->word, t2->word) > 0) {
				temp = t1->word;
				t1->word = t2->word;
				t2->word = temp;
			}
		}
	}
}


	
void
usage(int argc, char **argv)
{
	fprintf(stderr, "usage: %s <string>\n", argv[0]);
	exit(1);
}

uint64_t
to_microsec(struct timeval *tv)
{
	return (tv->tv_sec * 1000000L + tv->tv_usec);
}

void
print_wordlist(struct list *head)
{
	struct list *temp;

	for (temp = head; temp; temp = temp->next) {
		printf("%s\n", temp->word);
	}
}

int
validate_input(char *str)
{
	char *p;

	for (p = str; *p != '\0'; p++) {
		if (!isalpha(*p)) {
			return (1);
		}
	}
	return (0);
}

int
main(int argc, char **argv)
{
	int ret;
	char temp[MAX_WORD_SIZE];
	struct timeval c_start, c_end;
	struct timeval a_start, a_end;
	struct timeval s_start, s_end;
	uint64_t c_time, a_time, s_time;

	/*
	 * TODO:
	 * 1. use getopt() for options
	 * 2. All word combinations (not just equal sized anagrams)
	 * 3. Generate only word list
	 * 4. Generate only anagrams
	 * 5. Display time taken for different actions
	 * 6. Accept alternate/additional word databases
	 */
	if (argc != 2) {
		usage(argc, argv);
	}

	if (validate_input(argv[1]) != 0) {
		fprintf(stderr, "Non-alphabetic input. Exiting...\n");
		exit(1);
	}

	init_tree(&th);
	populate_tree(&th);
	init_stack();

	while(1) {
		//query_word_from_user(temp);
		/* Using strcpy since the input is sanitized via fgets */
		//strcpy(copy, temp);
		strncpy(copy, argv[1], MAX_WORD_SIZE);

		gettimeofday(&c_start, NULL);
		get_all_permutations(&copy[0], strlen(copy));
		gettimeofday(&c_end, NULL);

		gettimeofday(&s_start, NULL);
		sort_word_list(word_list_head);
		gettimeofday(&s_end, NULL);

		printf("\n\nPrinting sorted wordlist..\n");
		print_wordlist(word_list_head);

		printf("\n\nGenerating anagrams..\n");
		gettimeofday(&a_start, NULL);
		get_anagrams(word_list_head, strlen(copy), copy);
		gettimeofday(&a_end, NULL);

		cleanup_lists();
		break;
	}

	c_time = to_microsec(&c_end) - to_microsec(&c_start);
	s_time = to_microsec(&s_end) - to_microsec(&s_start);
	a_time = to_microsec(&a_end) - to_microsec(&a_start);
	printf("\n\n");
	fprintf(stderr, "time in microseconds for word combinations : %lu\n", c_time);
	fprintf(stderr, "time in microseconds for sort : %lu\n", s_time);
	fprintf(stderr, "time in microseconds for anagrams : %lu\n", a_time);

	return (0);
}
