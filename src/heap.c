/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:04 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:07 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	heap_before(t_sim *sim, int a, int b)
{
	t_coder	*x;
	t_coder	*y;

	x = &sim->coders[a];
	y = &sim->coders[b];
	if (sim->cfg.policy == EDF
		&& x->request_deadline != y->request_deadline)
		return (x->request_deadline < y->request_deadline);
	if (sim->cfg.policy == EDF && edf_slot(x) != edf_slot(y))
		return (edf_slot(x) < edf_slot(y));
	if (x->request_seq != y->request_seq)
		return (x->request_seq < y->request_seq);
	return (x->id < y->id);
}

void	heap_swap(int *a, int *b)
{
	int	tmp;

	tmp = *a;
	*a = *b;
	*b = tmp;
}

void	heap_push(t_sim *sim, t_heap *heap, int id)
{
	int	i;
	int	parent;

	i = heap->size++;
	heap->ids[i] = id;
	while (i > 0)
	{
		parent = (i - 1) / 2;
		if (!heap_before(sim, heap->ids[i], heap->ids[parent]))
			break ;
		heap_swap(&heap->ids[i], &heap->ids[parent]);
		i = parent;
	}
}

int	heap_top(t_heap *heap)
{
	if (heap->size == 0)
		return (-1);
	return (heap->ids[0]);
}
