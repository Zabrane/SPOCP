/***************************************************************************
                         spocptest.c  -  sends queries to a Spocp server
                             -------------------

    begin                : Wed Oct 5 2003
    copyright            : (C) 2003 by Stockholm University, Sweden
    email                : roland@catalogix.se

   COPYING RESTRICTIONS APPLY.  See COPYRIGHT File in top level directory
   of this package for details.

 **************************************************************************/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <log.h>
#include "spocpcli.h"

/*==========================================================================*/
/*
 Expected usage:

 spocptest spocpserver *( spocpquery ) 

 If spocpquery is missing it will take queries from STDIN 
 */

char *label(int code);

static void
print_usage(char *prog)
{
	printf("Usage: %s [-d] [-a] -s spocpserver 1*( spocpquery )\n", prog);
	printf("\t or %s [-d] [-a] -s spocpserver < file \n", prog);
}

char *OK = "OK";
char *DENIED = "DENIED";

char *label(int code)
{
    if (code == SPOCP_SUCCESS) {
        return OK;
    }
    else {
        return DENIED;
    }
}

int
main(int argc, char **argv)
{
	char        buf[BUFSIZ], *cp, *sp, *tmp;
	int         i = 0, ok = 0, r, j, test=0;
	SPOCP       *spocp;
	char		*sserv = NULL ;
	queres_t	qres;
	octet_t		**o;
	octet_t		*query, *path = 0;
	int         advanced = 0;

	spocpc_debug = 0;

	while ((i = getopt(argc, argv, "adhs:t")) != EOF) {
		switch (i) {

		case 'd':
			spocpc_debug = 1;
			break;

		case 's':
			sserv = strdup(optarg);
			break;

		case 'a':
			advanced = 1;
			break;

        case 't':
            test = 1;
            break;
                
		case 'h':
			print_usage(argv[0]);
			exit(0);
		}
	}

	if (sserv == NULL) {
		fprintf(stderr,"You have to give me a server to talk to\n");
		exit(1);
	}

	if ((spocp = spocpc_open(0, sserv, 3)) == 0) {
		fprintf(stderr, "Could not open connection to \"%s\"\n", sserv);
		exit(1);
	}
	if (spocpc_debug)
		traceLog(LOG_DEBUG,"Spocpserver [%s]", sserv);

	argc -= optind;
	argv += optind;
    
	if (argc) {
		for (i = 0; i < argc; i++) {
			if (advanced)
				query = oct_new(0,argv[i]);
			else
				query = sexp_normalize( argv[i] );

			if( query == 0 ) {
				fprintf(stderr,"[%s] not a s-expression\n", argv[i]);
				continue ;
			}

			memset(&qres, 0, sizeof(queres_t));

			r = spocpc_send_query(spocp, path, query, &qres) ;

			if ( r == SPOCPC_OK && qres.rescode == SPOCP_SUCCESS) {
				if (qres.blob) {
					for (o = qres.blob->arr, j=0 ;
					    j < qres.blob->n ; o++, j++) {
						tmp = oct2strdup(*o, '\\');
						printf("[%s] ", tmp);
						free(tmp);
					}
					octarr_free(qres.blob);
				}
				printf("OK\n");
			} else
				printf("DENIED\n");

			oct_free(query);
		}
	} else {
		if (spocpc_debug)
			traceLog(LOG_DEBUG,"reading stdin");
		while (fgets(buf, BUFSIZ, stdin)) {
			cp = &buf[strlen(buf) - 1];
			while (*cp == '\n' || *cp == '\r' || *cp == ' ')
				*cp-- = '\0';

			if (spocpc_debug)
				traceLog(LOG_DEBUG,"Got [%s]", buf);

			if (strncmp(buf, "OK ", 3) == 0) {
				ok = SPOCP_SUCCESS;
				sp = buf + 3;
			} else if (strncmp(buf, "DENIED ", 7) == 0) {
				ok = SPOCP_DENIED;
				sp = buf + 7;
			} else {
				ok = -1;
				sp = buf;
			}

			if (advanced)
				query = oct_new(0, sp);
			else
				query = sexp_normalize(sp);

			if( query == 0 ) {
				fprintf(stderr,"[%s] not a s-expression\n", sp);
				continue ;
			}

			memset(&qres, 0, sizeof(queres_t));

			/* this routine sends the query and will not return until it has 
			 * an answer */
			r = spocpc_send_query(spocp, path, query, &qres);

			if (r == SPOCPC_OK) {
				if (ok >= 0 ) {
                    if (qres.rescode != ok) {
                        tmp = oct2strdup(query, 0);
                        printf("\n%s != %s on %s\n",
                               label(qres.rescode), label(ok),  tmp);
                        free(tmp);
                    }
                    else {
                        printf(".");
                    }
				}
				else {
					if ( qres.rescode == SPOCP_SUCCESS) {
						if (qres.blob) {
							for (o = qres.blob->arr, j=0 ;
					   		 j < qres.blob->n ; o++, j++) {
								tmp = oct2strdup(*o, '\\');
								printf("[%s] ", tmp);
								free(tmp);
							}
						}
						printf("OK\n");
					} else
						printf("DENIED\n");
				}

				if (qres.blob)
					octarr_free( qres.blob );

			}
			oct_free(query);
		}
	}

    printf("\n");
	spocpc_send_logout(spocp);
	spocpc_close(spocp);
	free_spocp(spocp);

	exit(0);
}
