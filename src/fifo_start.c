/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   fifo_start.c                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:36:16 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:36:35 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	queue_first_fifo_locked(t_coder *coder)
{
	t_sim	*sim;
	int		rank;

	sim = coder->sim;
	rank = (coder->id - 1) / 2;
	if (coder->id % 2 == 0)
		rank += sim->cfg.count / 2;
	while (!sim->stop && sim->fifo_queued != rank)
		pthread_cond_wait(&sim->changed, &sim->mutex);
	if (sim->stop)
		return ;
	queue_request_locked(coder);
	sim->fifo_queued++;
	pthread_cond_broadcast(&sim->changed);
	while (!sim->stop && sim->fifo_queued < sim->cfg.count)
		pthread_cond_wait(&sim->changed, &sim->mutex);
}
