/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   grant_wait.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:37:04 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:37:13 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static long long	cooldown_end_locked(t_coder *coder)
{
	t_sim		*sim;
	t_dongle	*a;
	t_dongle	*b;
	long long	end;

	sim = coder->sim;
	lock_pair(sim, coder->left, coder->right);
	a = &sim->dongles[coder->left];
	b = &sim->dongles[coder->right];
	end = 0;
	if (a->owner == -1 && b->owner == -1)
	{
		end = a->ready_at;
		if (b->ready_at > end)
			end = b->ready_at;
	}
	unlock_pair(sim, coder->left, coder->right);
	return (end);
}

long long	next_cooldown_locked(t_coder *coder)
{
	long long	end;
	long long	remaining;

	end = cooldown_end_locked(coder);
	if (coder->sim->cfg.policy == EDF && coder->compiles == 0
		&& edf_slot(coder) > end)
		end = edf_slot(coder);
	remaining = end - now_ms();
	if (remaining > 0)
		return (remaining);
	return (0);
}
