#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "fzy.h"
#include "tty.h"

#define INITIAL_CAPACITY 1
int choices_capacity = 0;
int choices_n = 0;
const char **choices = NULL;
double *choices_score = NULL;
size_t *choices_sorted = NULL;
size_t current_selection = 0;

void resize_choices(int new_capacity){
	choices = realloc(choices, new_capacity * sizeof(const char *));
	choices_score = realloc(choices_score, new_capacity * sizeof(double));
	choices_sorted = realloc(choices_sorted, new_capacity * sizeof(size_t));

	int i = choices_capacity;
	for(; i < new_capacity; i++){
		choices[i] = NULL;
	}
	choices_capacity = new_capacity;
}

void add_choice(const char *choice){
	if(choices_n == choices_capacity){
		resize_choices(choices_capacity * 2);
	}
	choices[choices_n++] = choice;
}

void read_choices(){
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	while ((read = getline(&line, &len, stdin)) != -1) {
		char *nl;
		if((nl = strchr(line, '\n')))
			*nl = '\0';

		add_choice(line);

		line = NULL;
	}
	free(line);
}

size_t choices_available = 0;

static int cmpchoice(const void *p1, const void *p2) {
	size_t idx1 = *(size_t *)p1;
	size_t idx2 = *(size_t *)p2;

	double score1 = choices_score[idx1];
	double score2 = choices_score[idx2];

	if(score1 == score2)
		return 0;
	else if(score1 < score2)
		return 1;
	else
		return -1;
}

void run_search(char *needle){
	current_selection = 0;
	choices_available = 0;
	int i;
	for(i = 0; i < choices_n; i++){
		if(has_match(needle, choices[i])){
			choices_score[i] = match(needle, choices[i]);
			choices_sorted[choices_available++] = i;
		}
	}

	qsort(choices_sorted, choices_available, sizeof(size_t), cmpchoice);
}

int search_size;
char search[4096] = {0};

void clear(tty_t *tty){
	fprintf(tty->fout, "%c%c0G", 0x1b, '[');
	int line = 0;
	while(line++ < 10 + 1){
		fprintf(tty->fout, "%c%cK\n", 0x1b, '[');
	}
	fprintf(tty->fout, "%c%c%iA", 0x1b, '[', line-1);
	fprintf(tty->fout, "%c%c0G", 0x1b, '[');
}

void draw_match(tty_t *tty, const char *choice, int selected){
	int n = strlen(search);
	size_t positions[n + 1];
	for(int i = 0; i < n + 1; i++)
		positions[i] = -1;

	match_positions(search, choice, &positions[0]);

	for(size_t i = 0, p = 0; choice[i] != '\0'; i++){
		if(positions[p] == i){
			fprintf(tty->fout, "%c%c33m", 0x1b, '[');
			p++;
		}else{
			fprintf(tty->fout, "%c%c39;49m", 0x1b, '[');
		}
		fprintf(tty->fout, "%c", choice[i]);
	}
	fprintf(tty->fout, "\n");
	fprintf(tty->fout, "%c%c0m", 0x1b, '[');
}

void draw(tty_t *tty){
	int line = 0;
	const char *prompt = "> ";
	clear(tty);
	fprintf(tty->fout, "%s%s\n", prompt, search);
	for(size_t i = 0; line < 10 && i < choices_available; i++){
		if(i == current_selection)
			fprintf(tty->fout, "%c%c7m", 0x1b, '[');
		draw_match(tty, choices[choices_sorted[i]], i == current_selection);
		line++;
	}
	fprintf(tty->fout, "%c%c%iA", 0x1b, '[', line + 1);
	fprintf(tty->fout, "%c%c%ziG", 0x1b, '[', strlen(prompt) + strlen(search) + 1);
	fflush(tty->fout);
}

void emit(tty_t *tty){
	/* ttyout should be flushed before outputting on stdout */
	fclose(tty->fout);

	if(choices_available){
		/* output the selected result */
		printf("%s\n", choices[choices_sorted[current_selection]]);
	}else{
		/* No match, output the query instead */
		printf("%s\n", search);
	}

	exit(EXIT_SUCCESS);
}

void run(tty_t *tty){
	run_search(search);
	char ch;
	do {
		draw(tty);
		ch = tty_getchar(tty);
		if(isprint(ch)){
			/* FIXME: overflow */
			search[search_size++] = ch;
			search[search_size] = '\0';
			run_search(search);
		}else if(ch == 127 || ch == 8){ /* DEL || backspace */
			if(search_size)
				search[--search_size] = '\0';
			run_search(search);
		}else if(ch == 21){ /* C-U */
			search_size = 0;
			search[0] = '\0';
			run_search(search);
		}else if(ch == 23){ /* C-W */
			if(search_size)
				search[--search_size] = '\0';
			while(search_size && !isspace(search[--search_size]))
				search[search_size] = '\0';
			run_search(search);
		}else if(ch == 14){ /* C-N */
			current_selection = (current_selection + 1) % 10;
		}else if(ch == 16){ /* C-P */
			current_selection = (current_selection + 9) % 10;
		}else if(ch == 10){ /* Enter */
			clear(tty);
			emit(tty);
		}
	}while(1);
}

void usage(const char *argv0){
	fprintf(stderr, "USAGE: %s\n", argv0);
	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
	if(argc == 2 && !strcmp(argv[1], "-v")){
		printf("%s " VERSION  " (c) 2014 John Hawthorn\n", argv[0]);
		exit(EXIT_SUCCESS);
	}else if(argc != 1){
		usage(argv[0]);
	}
	tty_t tty;
	tty_init(&tty);

	resize_choices(INITIAL_CAPACITY);
	read_choices();

	clear(&tty);
	run(&tty);

	return 0;
}
