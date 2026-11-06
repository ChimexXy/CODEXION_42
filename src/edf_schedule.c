/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   edf_schedule.c                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:36:57 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:37:01 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	init_schedule(t_sim *sim)
{
	long long	cycle;
	long long	resource;
	int			half;

	half = sim->cfg.count / 2;
	if (half == 0)
		return ;
	resource = (long long)sim->cfg.compile + sim->cfg.cooldown;
	cycle = (resource * sim->cfg.count + half - 1) / half;
	if (cycle < (long long)sim->cfg.compile + sim->cfg.debug
		+ sim->cfg.refactor)
		cycle = (long long)sim->cfg.compile + sim->cfg.debug
			+ sim->cfg.refactor;
	if (cycle + 4 < sim->cfg.burnout)
		cycle += 4;
	sim->cycle = cycle;
}

long long	edf_slot(t_coder *coder)
{
	long long	phase;
	long long	index;
	t_sim		*sim;

	sim = coder->sim;
	if (sim->cfg.count < 2)
		return (sim->start);
	index = ((long long)(coder->id - 1) * (sim->cfg.count / 2))
		% sim->cfg.count;
	phase = sim->cycle * index / sim->cfg.count;
	return (sim->start + phase + sim->cycle * coder->compiles);
}
