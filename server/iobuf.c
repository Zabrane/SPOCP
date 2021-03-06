
/***************************************************************************
                           iobuf.c  -  description
                             -------------------
    begin                : Sat Oct 12 2002
    copyright            : (C) 2002 by Ume� University, Sweden
    email                : roland@catalogix.se

   COPYING RESTRICTIONS APPLY.  See COPYRIGHT File in top level directory
   of this package for details.

 **************************************************************************/

#include "locl.h"
RCSID("$Id$");

spocp_iobuf_t  *
iobuf_new(size_t size)
{
	spocp_iobuf_t  *io;

	io = (spocp_iobuf_t *) Calloc(1, sizeof(spocp_iobuf_t));

	io->p = io->w = io->r = io->buf = (char *) Calloc(size, sizeof(char));

	io->left = size - 1;	/* leave place for a terminating '\0' */
	io->bsize = size;

	io->end = io->buf + io->bsize;
	*io->w = '\0';

	pthread_mutex_init(&io->lock, NULL);
	pthread_cond_init(&io->empty, NULL);

	return io;
}

void 
iobuf_free( spocp_iobuf_t *io)
{
	if (io) {
		if (io->bsize)
			Free( io->buf );
		Free(io);
	}
}

spocp_result_t
iobuf_insert(spocp_iobuf_t * io, char *where, char *src, int srclen)
{
	size_t          dlen;
	spocp_result_t	rc = SPOCP_SUCCESS ;

	pthread_mutex_lock(&io->lock);

        if (io->left < (unsigned int) srclen )
		if((rc = iobuf_resize( io, srclen, 0 )) != SPOCP_SUCCESS) 
		    goto unlock;

	/* the number of bytes to move */
	dlen = io->w - where;

	memmove(where + srclen, where, dlen);
	memcpy(where, src, srclen);

	io->w += srclen;
	io->left -= srclen ;

 unlock:
	pthread_mutex_unlock(&io->lock);

	return rc ;
}

spocp_result_t
iobuf_add_len_tag( spocp_iobuf_t *io )
{
	unsigned int	len ;
	char 		ldef[16];
	int 		nr;
	spocp_result_t	sr;

	if (io == 0)
		return SPOCP_SUCCESS;

	len = io->w - io->p ;
	nr = snprintf(ldef, 16, "%u:", len);

	sr = iobuf_insert(io, io->p, ldef, nr);
	if( sr == SPOCP_SUCCESS) 
		io->p = io->w;

	return sr;
}

void
iobuf_shift(spocp_iobuf_t * io)
{
	int             len, last;

	/*
	 * LOG( SPOCP_DEBUG ) traceLog(LOG_DEBUG, "Shifting buffer b:%p r:%p w:%p
	 * left:%d bsize:%d", io->buf, io->r, io->w, io->left, io->bsize ) ; 
	 */

	pthread_mutex_lock(&io->lock);

	while (io->r <= io->w && WHITESPACE(*io->r))
		io->r++;

/*
	traceLog(LOG_DEBUG,"is10 [buf]%p [p]%p [r]%p [w]%p [left]%d",
	    io->buf, io->p, io->r, io->w, io->left);
	traceLog(LOG_DEBUG, "\t[%d][%d][%d][%d]", io->r - io->buf, io->w - io->r,
	    io->w - io->buf, io->left );
*/

	if (io->r >= io->w) {	/* nothing in buffer */
		io->p = io->r = io->w = io->buf;
		io->left = io->bsize - 1;
		*io->w = 0;
		/*
		 * traceLog(LOG_DEBUG, "DONE Shifting buffer b:%p r:%p w:%p left:%d",
		 * io->buf, io->r, io->w, io->left ) ; 
		 */
	} else {		/* something in the buffer, shift it to the
				 * front */
		len = io->w - io->r;
		last = io->w - io->p;
		memmove(io->buf, io->r, len);
		io->r = io->buf;
		io->w = io->buf + len;
		io->p = io->buf + last;
		io->left = io->bsize - len - 1;
		*io->w = '\0';
	}

	DEBUG(SPOCP_DSRV)
		traceLog(LOG_DEBUG,"DONE Shifting buffer b:%p r:%p w:%p left:%d",
		    io->buf, io->r, io->w, io->left);

/*
	traceLog(LOG_DEBUG,"is11 [buf]%p [p]%p [r]%p [w]%p [left]%d",
	    io->buf, io->p, io->r, io->w, io->left);
	traceLog(LOG_DEBUG, "\t[%d][%d][%d][%d]", io->r - io->buf, io->w - io->r,
	    io->w - io->buf, io->left );
*/

	pthread_mutex_unlock(&io->lock);

}

spocp_result_t
iobuf_resize(spocp_iobuf_t * io, int increase, int lock)
{
	int             nr, nw, np, ret;
	char           *tmp;

	if (io == 0) {
		ret = SPOCP_OPERATIONSERROR;
		goto dontunlock;
	}

	if (io->bsize == SPOCP_MAXBUF) {
		ret = SPOCP_BUF_OVERFLOW;
		goto dontunlock;
	}

	if (lock)
		pthread_mutex_lock(&io->lock);

	nr = io->bsize;

	if (increase) {
		if (io->bsize + increase > SPOCP_MAXBUF) {
			ret = SPOCP_BUF_OVERFLOW;
			goto unlock;
		}
		else if (increase < SPOCP_IOBUFSIZE)
			io->bsize += SPOCP_IOBUFSIZE;
		else
			io->bsize += increase;
	} else {
		if (io->bsize + SPOCP_IOBUFSIZE > SPOCP_MAXBUF)
			io->bsize += SPOCP_MAXBUF;
		else
			io->bsize += SPOCP_IOBUFSIZE;
	}

	/*
	 * the increase should be usable 
	 */
	io->left += io->bsize - nr;

	nr = io->r - io->buf;
	nw = io->w - io->buf;
	np = io->p - io->buf;

	tmp = Realloc(io->buf, io->bsize);

	io->buf = tmp;
	io->end = io->buf + io->bsize -1;
	*io->end = '\0';
	io->r = io->buf + nr;
	io->w = io->buf + nw;
	io->p = io->buf + np;

	ret = SPOCP_SUCCESS;
	
	unlock:
	if (lock)
		pthread_mutex_unlock(&io->lock);

	dontunlock:
	return ret;
}

spocp_result_t
iobuf_add(spocp_iobuf_t * io, char *s)
{
	spocp_result_t  rc = SPOCP_SUCCESS;
	int             n;

	if (s == 0 || *s == 0)
		return SPOCP_SUCCESS;	/* no error */

	if (io == 0)
		return SPOCP_SUCCESS; /* Eh ? */

	n = strlen(s);

	/*
	 * LOG( SPOCP_DEBUG ) traceLog(LOG_DEBUG, "Add to IObuf \"%s\" %d/%d", s, n,
	 * io->left) ; 
	 */

	pthread_mutex_lock(&io->lock);

	/*
	 * are the place enough ? If not make some 
	 */
	if ((int) io->left < n)
		if ((rc = iobuf_resize(io, n - io->left, 0)) != SPOCP_SUCCESS)
		    goto unlock;

	memcpy(io->w, s, n);

	io->w += n;
	*io->w = '\0';
	io->left -= n;

 unlock:

	pthread_mutex_unlock(&io->lock);

	return rc;
}

spocp_result_t
iobuf_addn(spocp_iobuf_t * io, char *s, size_t n)
{
	spocp_result_t  rc = SPOCP_SUCCESS;

	if (s == 0 || *s == 0)
		return SPOCP_SUCCESS;	/* no error */

	pthread_mutex_lock(&io->lock);

	/*
	 * are the place enough ? If not make some 
	 */
	if ( io->left < n)
		if ((rc = iobuf_resize(io, n - io->left, 0)) != SPOCP_SUCCESS)
		    goto unlock;

	memcpy(io->w, s, n);

	io->w += n;
	*io->w = '\0';
	io->left -= n;

 unlock:
	pthread_mutex_unlock(&io->lock);
	return rc;
}

spocp_result_t
iobuf_add_octet(spocp_iobuf_t * io, octet_t * s)
{
	spocp_result_t  rc = SPOCP_SUCCESS;
	size_t          l, lf;
	char            lenfield[8];

	if (s == 0 || s->len == 0)
		return SPOCP_SUCCESS;	/* adding nothing is no error */

	if (io == 0)
		return SPOCP_SUCCESS; /* happens with native_server */ 

	/*
	 * the complete length of the bytestring, with length field 
	 */
	lf = DIGITS(s->len) + 1;
	l = s->len + lf;

	pthread_mutex_lock(&io->lock);

	/*
	 * are the place enough ? If not make some 
	 */
	if (io->left < l)
		if ((rc = iobuf_resize(io, l - io->left, 0)) != SPOCP_SUCCESS)
		    goto unlock;

	snprintf(lenfield, 8, "%u:", (unsigned int) s->len);

	/* Flawfinder: ignore */
	strcpy(io->w, lenfield);
	io->w += lf;

	memcpy(io->w, s->val, s->len);
	io->w += s->len;

	*io->w = '\0';
	io->left -= l;


 unlock:
	pthread_mutex_unlock(&io->lock);

	return rc;
}

void
iobuf_flush(spocp_iobuf_t * io)
{
	pthread_mutex_lock(&io->lock);

	io->p = io->r = io->w = io->buf;
	*io->w = 0;
	io->left = io->bsize - 1;

	pthread_mutex_unlock(&io->lock);

}

int
iobuf_content(spocp_iobuf_t * io)
{
	int             n;

	if (io == 0)
		return -1;

	pthread_mutex_lock(&io->lock);
	n = (io->w - io->r);
	pthread_mutex_unlock(&io->lock);

	return n;
}

void 
iobuf_print( char *tag, spocp_iobuf_t *io )
{
	octet_t o;

	if (io != 0 ) {
		o.val = io->r;
		o.len = io->w - io->r;
		oct_print( LOG_DEBUG, tag, &o );
	}
	else 
		traceLog(LOG_DEBUG,"%s: %s", tag, "Empty");

}

void
iobuf_info( spocp_iobuf_t *io )
{
	traceLog(LOG_DEBUG,"----iobuf info----");
	if (io) {
		traceLog(LOG_DEBUG,"bsize: %d", io->bsize );
		traceLog(LOG_DEBUG,"left: %d", io->left);
		traceLog(LOG_DEBUG,"left: %d", io->w - io->r);

		traceLog(LOG_DEBUG,"buf: %p", io->buf);
		traceLog(LOG_DEBUG,"r: %p", io->r);
		traceLog(LOG_DEBUG,"w: %p", io->w);
		traceLog(LOG_DEBUG,"p: %p", io->p);
		traceLog(LOG_DEBUG,"end: %p", io->end);
	}
	else {
		traceLog(LOG_DEBUG,"NULL");
	}
	traceLog(LOG_DEBUG,"----end----");
}

