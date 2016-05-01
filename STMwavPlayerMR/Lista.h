#include "ff.h"

struct Lista
{
	FILINFO plik;
	struct Lista *next;
};
struct Lista *add_last(struct Lista *last, FILINFO data);
