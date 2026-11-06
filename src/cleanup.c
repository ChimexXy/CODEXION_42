/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cleanup.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:36:28 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:36:32 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	cleanup_sim(t_sim *sim)
{
	int	i;

	i = 0;
	while (i < sim->dongles_ready)
	{
		free(sim->dongles[i].queue.ids);
		pthread_mutex_destroy(&sim->dongles[i].mutex);
		i++;
	}
	free(sim->dongles);
	free(sim->coders);
	if (sim->cond_ready)
		pthread_cond_destroy(&sim->changed);
	if (sim->print_ready)
		pthread_mutex_destroy(&sim->print_mutex);
	if (sim->mutex_ready)
		pthread_mutex_destroy(&sim->mutex);
}
