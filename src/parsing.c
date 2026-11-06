/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parsing.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:26 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:29 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	parse_int(char *s, int *value)
{
	long long	n;
	int			i;

	if (!s || !s[0])
		return (0);
	n = 0;
	i = 0;
	while (s[i])
	{
		if (s[i] < '0' || s[i] > '9')
			return (0);
		n = n * 10 + s[i] - '0';
		if (n > INT_MAX)
			return (0);
		i++;
	}
	*value = (int)n;
	return (1);
}

static int	parse_numbers(char **v, t_config *cfg)
{
	if (!parse_int(v[1], &cfg->count) || cfg->count == 0)
		return (0);
	if (!parse_int(v[2], &cfg->burnout))
		return (0);
	if (!parse_int(v[3], &cfg->compile))
		return (0);
	if (!parse_int(v[4], &cfg->debug))
		return (0);
	if (!parse_int(v[5], &cfg->refactor))
		return (0);
	if (!parse_int(v[6], &cfg->required))
		return (0);
	if (!parse_int(v[7], &cfg->cooldown))
		return (0);
	return (1);
}

int	parse_args(int argc, char **argv, t_config *cfg)
{
	if (argc != 9 || !parse_numbers(argv, cfg))
		return (0);
	if (strcmp(argv[8], "fifo") == 0)
		cfg->policy = FIFO;
	else if (strcmp(argv[8], "edf") == 0)
		cfg->policy = EDF;
	else
		return (0);
	return (1);
}
