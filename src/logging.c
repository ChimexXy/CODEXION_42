/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   logging.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: zhamza <zhamza@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/11/06 02:38:13 by zhamza            #+#    #+#             */
/*   Updated: 2026/11/06 02:38:16 by zhamza           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "codexion.h"

static int	append_number(char *buf, int pos, long long value)
{
	char	digits[24];
	int		i;

	i = 0;
	if (value == 0)
		digits[i++] = '0';
	while (value > 0)
	{
		digits[i++] = '0' + value % 10;
		value /= 10;
	}
	while (i > 0)
		buf[pos++] = digits[--i];
	return (pos);
}

static int	append_text(char *buf, int pos, char *text)
{
	int	i;

	i = 0;
	while (text[i])
		buf[pos++] = text[i++];
	return (pos);
}

static void	write_line(char *line, int length)
{
	int		pos;
	ssize_t	n;

	pos = 0;
	while (pos < length)
	{
		n = write(1, line + pos, (size_t)(length - pos));
		if (n <= 0)
			break ;
		pos += (int)n;
	}
}

void	log_locked(t_sim *sim, int id, char *message)
{
	char	line[128];
	int		pos;

	pos = 0;
	pthread_mutex_lock(&sim->print_mutex);
	pos = append_number(line, pos, now_ms() - sim->start);
	line[pos++] = ' ';
	pos = append_number(line, pos, id);
	line[pos++] = ' ';
	pos = append_text(line, pos, message);
	line[pos++] = '\n';
	write_line(line, pos);
	pthread_mutex_unlock(&sim->print_mutex);
}
