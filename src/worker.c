/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   worker.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:39:06 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:39:53 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	begin_coder(t_sim *sim)
{
	int	active;

	pthread_mutex_lock(&sim->mutex);
	while (!sim->ready)
		pthread_cond_wait(&sim->changed, &sim->mutex);
	active = !sim->stop;
	pthread_mutex_unlock(&sim->mutex);
	return (active);
}

static int	all_completed(t_sim *sim)
{
	int	i;

	i = 0;
	while (i < sim->cfg.count)
	{
		if (sim->coders[i].compiles < sim->cfg.required)
			return (0);
		i++;
	}
	return (1);
}

static int	end_compile(t_coder *coder)
{
	t_sim	*sim;
	int		active;

	sim = coder->sim;
	pthread_mutex_lock(&sim->mutex);
	release_dongles_locked(coder);
	if (!check_burnout_locked(sim))
	{
		if (coder->compiles < sim->cfg.required)
			coder->compiles++;
		if (all_completed(sim))
			sim->stop = 1;
		else
			log_locked(sim, coder->id, "is debugging");
	}
	active = !sim->stop;
	pthread_cond_broadcast(&sim->changed);
	pthread_mutex_unlock(&sim->mutex);
	return (active);
}

static int	compile_phase(t_coder *coder)
{
	if (!wait_phase(coder->sim, coder->sim->cfg.compile))
	{
		end_compile(coder);
		return (0);
	}
	return (end_compile(coder));
}

void	*coder_thread(void *arg)
{
	t_coder		*coder;
	t_sim		*sim;

	coder = (t_coder *)arg;
	sim = coder->sim;
	if (!begin_coder(sim))
		return (NULL);
	while (request_dongles(coder))
	{
		if (!compile_phase(coder))
			break ;
		if (!wait_phase(sim, sim->cfg.debug))
			break ;
		if (!begin_refactor(coder))
			break ;
		if (!wait_phase(sim, sim->cfg.refactor))
			break ;
	}
	return (NULL);
}
