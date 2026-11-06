/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   timing.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:34 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:37 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

long long	now_ms(void)
{
	struct timespec	ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ((long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void	wait_delta_locked(t_sim *sim, long long delta)
{
	struct timespec	until;

	if (delta <= 0)
		return ;
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_sec += delta / 1000;
	until.tv_nsec += (delta % 1000) * 1000000;
	if (until.tv_nsec >= 1000000000)
	{
		until.tv_sec++;
		until.tv_nsec -= 1000000000;
	}
	pthread_cond_timedwait(&sim->changed, &sim->mutex, &until);
}

int	wait_phase(t_sim *sim, int duration)
{
	long long	end;
	long long	remaining;

	end = now_ms() + duration;
	pthread_mutex_lock(&sim->mutex);
	while (!sim->stop)
	{
		remaining = end - now_ms();
		if (remaining <= 0)
			break ;
		wait_delta_locked(sim, remaining);
	}
	remaining = !sim->stop;
	pthread_mutex_unlock(&sim->mutex);
	return ((int)remaining);
}
