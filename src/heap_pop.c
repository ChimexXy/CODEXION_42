/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   heap_pop.c                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:37:58 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:02 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static void	sift_down(t_sim *sim, t_heap *heap, int parent)
{
	int	child;
	int	best;

	while (parent * 2 + 1 < heap->size)
	{
		child = parent * 2 + 1;
		best = child;
		if (child + 1 < heap->size
			&& heap_before(sim, heap->ids[child + 1], heap->ids[child]))
			best = child + 1;
		if (!heap_before(sim, heap->ids[best], heap->ids[parent]))
			break ;
		heap_swap(&heap->ids[parent], &heap->ids[best]);
		parent = best;
	}
}

int	heap_pop(t_sim *sim, t_heap *heap)
{
	int	result;

	if (heap->size == 0)
		return (-1);
	result = heap->ids[0];
	heap->size--;
	if (heap->size > 0)
	{
		heap->ids[0] = heap->ids[heap->size];
		sift_down(sim, heap, 0);
	}
	return (result);
}
