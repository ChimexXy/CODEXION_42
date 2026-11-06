/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   init.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:09 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:12 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	init_dongles(t_sim *sim)
{
	int	i;

	i = 0;
	while (i < sim->cfg.count)
	{
		if (pthread_mutex_init(&sim->dongles[i].mutex, NULL) != 0)
			return (0);
		sim->dongles_ready++;
		sim->dongles[i].owner = -1;
		sim->dongles[i].queue.capacity = 2;
		sim->dongles[i].queue.ids = malloc(2 * sizeof(int));
		if (!sim->dongles[i].queue.ids)
			return (0);
		i++;
	}
	return (1);
}

static void	init_coders(t_sim *sim)
{
	int	i;

	i = 0;
	while (i < sim->cfg.count)
	{
		sim->coders[i].id = i + 1;
		sim->coders[i].left = i;
		sim->coders[i].right = (i + 1) % sim->cfg.count;
		sim->coders[i].sim = sim;
		i++;
	}
}

static int	init_controls(t_sim *sim)
{
	if (pthread_mutex_init(&sim->mutex, NULL) != 0)
		return (0);
	sim->mutex_ready = 1;
	if (pthread_mutex_init(&sim->print_mutex, NULL) != 0)
		return (0);
	sim->print_ready = 1;
	if (pthread_cond_init(&sim->changed, NULL) != 0)
		return (0);
	sim->cond_ready = 1;
	return (1);
}

int	init_sim(t_sim *sim, t_config *cfg)
{
	memset(sim, 0, sizeof(*sim));
	sim->cfg = *cfg;
	if (!init_controls(sim))
		return (0);
	sim->dongles = malloc((size_t)cfg->count * sizeof(t_dongle));
	sim->coders = malloc((size_t)cfg->count * sizeof(t_coder));
	if (!sim->dongles || !sim->coders)
		return (0);
	memset(sim->dongles, 0, (size_t)cfg->count * sizeof(t_dongle));
	memset(sim->coders, 0, (size_t)cfg->count * sizeof(t_coder));
	if (!init_dongles(sim))
		return (0);
	init_coders(sim);
	return (1);
}
