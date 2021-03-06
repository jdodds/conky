/* -*- mode: c++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*-
 * vim: ts=4 sw=4 noet ai cindent syntax=cpp
 *
 * Conky, a system monitor, based on torsmo
 *
 * Please see COPYING for details
 *
 * Copyright (c) 2005-2010 Brenden Matthews, Philip Kovacs, et. al.
 *	(see AUTHORS)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "conky.h"
#include "logging.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "text_object.h"
#include <libircclient/libircclient.h>

struct ll_text {
	char *text;
	struct ll_text *next;
};

struct obj_irc {
	pthread_t *thread;
	irc_session_t *session;
	char *arg;
};

struct ctx {
	char *chan;
	struct ll_text *messages;
};

void ev_connected(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count) {
	struct ctx *ctxptr = (struct ctx *) irc_get_ctx(session);
	if(irc_cmd_join(session, ctxptr->chan, NULL) != 0) {
		NORM_ERR("irc: %s", irc_strerror(irc_errno(session)));
	}
	if(event || origin || params || count) {}	//fix gcc warnings
}

void addmessage(struct ctx *ctxptr, char *nick, const char *text) {
	struct ll_text *lastmsg = ctxptr->messages;
	struct ll_text *newmsg = (struct ll_text*) malloc(sizeof(struct ll_text));
	newmsg->text = (char*) malloc(strlen(nick) + strlen(text) + 4);	//4 = ": \n"
	sprintf(newmsg->text, "%s: %s\n", nick, text);
	newmsg->next = NULL;
	if(!lastmsg) {
		ctxptr->messages = newmsg;
	} else {
		while(lastmsg->next) {
			lastmsg = lastmsg->next;
		}
		lastmsg->next = newmsg;
	}
}

void ev_talkinchan(irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count) {
	char nickname[64];
	struct ctx *ctxptr = (struct ctx *) irc_get_ctx(session);

	irc_target_get_nick(origin, nickname, sizeof(nickname));
	addmessage(ctxptr, nickname, params[1]);
	if(session || event || count) {}	//fix gcc warnings
}

void ev_num(irc_session_t *session, unsigned int event, const char *origin, const char **params, unsigned int count) {
	char attachment[4]="_00";

	if(event == 433) {	//nick in use
		char *newnick = (char*) malloc(strlen(params[1]) + 4);
		strcpy(newnick, params[1]);
		attachment[1] += rand() % 10;
		attachment[2] += rand() % 10;
		strcat(newnick, attachment);
		irc_cmd_nick(session, newnick);
		free(newnick);
	}
	if(origin || count) {}	//fix gcc warnings
}

#define IRCSYNTAX "The correct syntax is ${irc server(:port) #channel}"
#define IRCPORT 6667
#define IRCNICK "conky"
#define IRCSERVERPASS NULL
#define IRCUSER NULL
#define IRCREAL NULL

void *ircclient(void *ptr) {
	struct obj_irc *ircobj = (struct obj_irc *) ptr;
	struct ctx *ctxptr = (struct ctx *) malloc(sizeof(struct ctx));
	irc_callbacks_t callbacks;
	char *server;
	char *strport;
	unsigned int port;

	memset (&callbacks, 0, sizeof(callbacks));
	callbacks.event_connect = ev_connected;
	callbacks.event_channel = ev_talkinchan;
	callbacks.event_numeric = ev_num;
	ircobj->session = irc_create_session(&callbacks);
	server = strtok(ircobj->arg , " ");
	ctxptr->chan = strtok(NULL , " ");
	if( ! ctxptr->chan) {
		NORM_ERR("irc: %s", IRCSYNTAX);
	}
	ctxptr->messages = NULL;
	irc_set_ctx(ircobj->session, ctxptr);
	server = strtok(server, ":");
	strport = strtok(NULL, ":");
	if(strport) {
		port = strtol(strport, NULL, 10);
		if(port < 1 || port > 65535)
			port = IRCPORT;
	} else {
		port = IRCPORT;
	}
	if(irc_connect(ircobj->session, server, port, IRCSERVERPASS, IRCNICK, IRCUSER, IRCREAL) != 0) {
		NORM_ERR("irc: %s", irc_strerror(irc_errno(ircobj->session)));
	}
	if(irc_run(ircobj->session) != 0) {
		int ircerror = irc_errno(ircobj->session);
		if(irc_is_connected(ircobj->session)) {
			NORM_ERR("irc: %s", irc_strerror(ircerror));
		} else {
			NORM_ERR("irc: disconnected");
		}
	}
	free(ircobj->arg);
	free(ctxptr);
	return NULL;
}

void parse_irc_args(struct text_object *obj, const char* arg) {
	struct obj_irc* opaque = (struct obj_irc *) malloc(sizeof(struct obj_irc));
	opaque->thread = (pthread_t *) malloc(sizeof(pthread_t));
	srand(time(NULL));
	opaque->session = NULL;
	opaque->arg = strdup(arg);
	pthread_create(opaque->thread, NULL, ircclient, opaque);
	obj->data.opaque = opaque;
}

void print_irc(struct text_object *obj, char *p, int p_max_size) {
	struct obj_irc *ircobj = (struct obj_irc *) obj->data.opaque;
	struct ctx *ctxptr;
	struct ll_text *nextmsg, *curmsg;

	if( ! ircobj->session) return;
	if( ! irc_is_connected(ircobj->session)) return;
	ctxptr = (struct ctx *) irc_get_ctx(ircobj->session);
	curmsg = ctxptr->messages;
	while(curmsg) {
		nextmsg = curmsg->next;
		strncat(p, curmsg->text, p_max_size - strlen(p) - 1);
		free(curmsg->text);
		free(curmsg);
		curmsg = nextmsg;
	}
	if(p[0] != 0) {
		p[strlen(p) - 1] = 0;
	}
	ctxptr->messages = NULL;
}

void free_irc(struct text_object *obj) {
	struct obj_irc *ircobj = (struct obj_irc *) obj->data.opaque;
	struct ctx *ctxptr;
	struct ll_text *nextmsg, *curmsg = NULL;

	if(ircobj->session) {
		if( irc_is_connected(ircobj->session)) {
			ctxptr = (struct ctx *) irc_get_ctx(ircobj->session);
			curmsg = ctxptr->messages;
			irc_disconnect(ircobj->session);
		}
		pthread_join(*(ircobj->thread), NULL);
		irc_destroy_session(ircobj->session);
	}
	free(ircobj->thread);
	free(obj->data.opaque);
	while(curmsg) {
		nextmsg = curmsg->next;
		free(curmsg->text);
		free(curmsg);
		curmsg = nextmsg;
	}
}
