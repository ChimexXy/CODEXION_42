/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   threads.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:30 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:33 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static void	start_clock(t_sim *sim)
{
	int	i;

	pthread_mutex_lock(&sim->mutex);
	sim->start = now_ms();
	init_schedule(sim);
	i = 0;
	while (i < sim->cfg.count)
	{
		sim->coders[i].last_start = sim->start;
		if (sim->cfg.policy == EDF && sim->cfg.count > 1)
			queue_request_locked(&sim->coders[i]);
		i++;
	}
	sim->ready = 1;
	pthread_cond_broadcast(&sim->changed);
	pthread_mutex_unlock(&sim->mutex);
}

static void	stop_on_error(t_sim *sim)
{
	pthread_mutex_lock(&sim->mutex);
	sim->stop = 1;
	sim->ready = 1;
	pthread_cond_broadcast(&sim->changed);
	pthread_mutex_unlock(&sim->mutex);
}

static void	join_threads(t_sim *sim)
{
	int	i;

	i = 0;
	while (i < sim->coders_created)
	{
		pthread_join(sim->coders[i].thread, NULL);
		i++;
	}
	if (sim->monitor_created)
		pthread_join(sim->monitor, NULL);
}

int	run_sim(t_sim *sim)
{
	int	i;

	if (sim->cfg.required == 0)
		return (1);
	i = 0;
	while (i < sim->cfg.count)
	{
		if (pthread_create(&sim->coders[i].thread, NULL,
				coder_thread, &sim->coders[i]) != 0)
			break ;
		sim->coders_created++;
		i++;
	}
	if (i == sim->cfg.count && pthread_create(&sim->monitor, NULL,
			monitor_thread, sim) == 0)
		sim->monitor_created = 1;
	if (!sim->monitor_created)
		stop_on_error(sim);
	else
		start_clock(sim);
	join_threads(sim);
	return (sim->monitor_created);
}
