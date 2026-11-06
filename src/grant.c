/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   grant.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:37:26 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:37:54 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	can_grant_locked(t_coder *coder, long long now)
{
	t_sim		*sim;
	t_dongle	*a;
	t_dongle	*b;
	int			available;

	sim = coder->sim;
	if (sim->cfg.policy == EDF && coder->compiles == 0
		&& now < edf_slot(coder))
		return (0);
	lock_pair(sim, coder->left, coder->right);
	a = &sim->dongles[coder->left];
	b = &sim->dongles[coder->right];
	available = (a->owner == -1 && b->owner == -1
			&& now >= a->ready_at && now >= b->ready_at
			&& heap_top(&a->queue) == coder->id - 1
			&& heap_top(&b->queue) == coder->id - 1);
	unlock_pair(sim, coder->left, coder->right);
	return (available);
}

static void	commit_grant_locked(t_coder *coder, long long start)
{
	t_sim	*sim;

	sim = coder->sim;
	heap_pop(sim, &sim->dongles[coder->left].queue);
	heap_pop(sim, &sim->dongles[coder->right].queue);
	sim->dongles[coder->left].owner = coder->id;
	sim->dongles[coder->right].owner = coder->id;
	coder->last_start = start;
}

int	try_grant_locked(t_coder *coder)
{
	t_sim		*sim;
	long long	start;

	sim = coder->sim;
	start = now_ms();
	if (start >= coder->last_start + sim->cfg.burnout
		|| !can_grant_locked(coder, start))
		return (0);
	lock_pair(sim, coder->left, coder->right);
	start = now_ms();
	if (start >= coder->last_start + sim->cfg.burnout)
	{
		unlock_pair(sim, coder->left, coder->right);
		return (0);
	}
	commit_grant_locked(coder, start);
	unlock_pair(sim, coder->left, coder->right);
	log_locked(sim, coder->id, "has taken a dongle");
	log_locked(sim, coder->id, "has taken a dongle");
	log_locked(sim, coder->id, "is compiling");
	pthread_cond_broadcast(&sim->changed);
	return (1);
}
