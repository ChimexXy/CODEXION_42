/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   dongle.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:36:47 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:36:51 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

void	lock_pair(t_sim *sim, int left, int right)
{
	if (left < right)
	{
		pthread_mutex_lock(&sim->dongles[left].mutex);
		pthread_mutex_lock(&sim->dongles[right].mutex);
	}
	else
	{
		pthread_mutex_lock(&sim->dongles[right].mutex);
		if (left != right)
			pthread_mutex_lock(&sim->dongles[left].mutex);
	}
}

void	unlock_pair(t_sim *sim, int left, int right)
{
	if (left < right)
	{
		pthread_mutex_unlock(&sim->dongles[right].mutex);
		pthread_mutex_unlock(&sim->dongles[left].mutex);
	}
	else
	{
		if (left != right)
			pthread_mutex_unlock(&sim->dongles[left].mutex);
		pthread_mutex_unlock(&sim->dongles[right].mutex);
	}
}

void	release_dongles_locked(t_coder *coder)
{
	t_sim		*sim;
	long long	ready_at;

	sim = coder->sim;
	ready_at = now_ms() + sim->cfg.cooldown;
	lock_pair(sim, coder->left, coder->right);
	sim->dongles[coder->left].owner = -1;
	sim->dongles[coder->left].ready_at = ready_at;
	sim->dongles[coder->right].owner = -1;
	sim->dongles[coder->right].ready_at = ready_at;
	unlock_pair(sim, coder->left, coder->right);
	pthread_cond_broadcast(&sim->changed);
}
