#include "Lista.h"

struct Lista *add_last(struct Lista *last, FILINFO data)
{
	struct Lista *wsk;
	wsk=(struct Lista*)malloc(sizeof(struct Lista));
	wsk->plik=data;
	wsk->next=0;
	if(last!=0) last->next=wsk;
	return wsk;
}
