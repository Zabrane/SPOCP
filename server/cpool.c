#include "locl.h"
RCSID("$Id$");

#define MAX_NUMBER_OF_CONNECTIONS 64

static afpool_t *
cpool_new(int nc)
{
	afpool_t       *afpool = 0;
	pool_item_t    *pi;
	pool_t         *pool;
	conn_t         *con;
	int             i;

	if (0)
		traceLog(LOG_INFO,"cpool_new\n");

	afpool = afpool_new();

	if (0)
		traceLog(LOG_INFO,"afpool_new\n");

	pool = afpool->free;

	if (0)
		traceLog(LOG_INFO,"max simultaneous connections: %d\n", nc);

	if (nc) {
		for (i = 0; i < nc; i++) {
			con = conn_new();

			pi = (pool_item_t *) Calloc(1, sizeof(pool_item_t));
			pi->info = (void *) con;

			pool_add(pool, pi);
		}
	}

	pthread_mutex_init(&(afpool->aflock), NULL);

	return afpool;
}

int
srv_init(srv_t * srv, char *cnfg)
{
	/*
	fprintf(stderr, "init_server\n");
	fprintf(stderr, "Read config\n");
	*/

	if (read_config(cnfg, srv) == 0)
		return -1;

	/*
	 * fprintf( stderr, "New ruleset\n" ) ; 
	 */

	if (srv->root == 0)
		srv->root = ruleset_new(0);

	pthread_mutex_init(&(srv->mutex), NULL);

	/*
	 * max number of simultaneously open connections 
	 */
	if (srv->nconn == 0)
		srv->nconn = MAX_NUMBER_OF_CONNECTIONS;

	srv->connections = cpool_new(srv->nconn);

	return 0;
}

static void
P_conn_free( void *v)
{
	if (v)
		conn_free( (conn_t *) v );
}

void
srv_free( srv_t *srv)
{
	if (srv->work)
		tpool_destroy( srv->work, 1);

	if (srv->root)
		ruleset_free( srv->root, 1 );

	if (srv->plugin) 
		plugin_unload_all( srv->plugin);

	if (srv->id)
		Free( srv->id );

	if (srv->certificateFile)
		Free( srv->certificateFile);

	if (srv->privateKey)
		Free( srv->privateKey);

	if (srv->caList)
		Free( srv->caList);

	if (srv->dhFile)
		Free( srv->dhFile );

	if (srv->SslEntropyFile)
		Free( srv->SslEntropyFile);

	if (srv->passwd)
		Free( srv->passwd );

	if (srv->logfile)
		Free( srv->logfile);

	if (srv->pidfile)
		Free( srv->pidfile );

	if (srv->uds)
		Free( srv->uds );

	if (srv->rulefile)
		Free( srv->rulefile) ;

	if (srv->server_id)
		Free( srv->server_id);
	
	if (srv->hostname)
		Free( srv->hostname);

	if (srv->connections)
		afpool_free( srv->connections, P_conn_free);


	memset( srv, 0, sizeof( srv_t )) ;
}

