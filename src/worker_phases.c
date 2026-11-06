/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   worker_phases.c                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:38 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:40:00 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

int	begin_refactor(t_coder *coder)
{
	t_sim	*sim;
	int		active;

	sim = coder->sim;
	pthread_mutex_lock(&sim->mutex);
	active = !sim->stop;
	if (active)
		log_locked(sim, coder->id, "is refactoring");
	pthread_mutex_unlock(&sim->mutex);
	return (active);
}
